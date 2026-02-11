// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023-2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/completion.h>
#include <linux/platform_device.h>
#include <linux/mailbox_controller.h>
#include <linux/mailbox_client.h>
#include <linux/mailbox/tmel_qmp.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/kthread.h>
#include <linux/workqueue.h>

#define QMP_NUM_CHANS	0x1
#define QMP_TOUT_MS	1000
#define QMP_TX_TOUT_MS	1000
#define QMP_LOG_BUF_SIZE	128
#define MBOX_ALIGN_BYTES	3
#define QMP_CTRL_DATA_SIZE	4

#define QMP_CH_VAR_GET(mdev, desc, var) \
	((mdev)->desc.bits.var)

#define QMP_CH_VAR_SET(mdev, desc, var) \
	do {(mdev)->desc.bits.var = 1;} while(0)

#define QMP_CH_VAR_CLR(mdev, desc, var) \
	do {(mdev)->desc.bits.var = 0;} while(0)

#define QMP_MCORE_CH_VAR_GET(mdev, var)	QMP_CH_VAR_GET(mdev, mcore, var)
#define QMP_MCORE_CH_VAR_SET(mdev, var)	QMP_CH_VAR_SET(mdev, mcore, var)
#define QMP_MCORE_CH_VAR_CLR(mdev, var)	QMP_CH_VAR_CLR(mdev, mcore, var)
#define QMP_MCORE_CH_VAR_TOGGLE(mdev, var) \
	do {(mdev)->mcore.bits.var = !((mdev)->mcore.bits.var);} while(0)

#define QMP_MCORE_CH_ACKED_CHECK(mdev, var) \
	((mdev)->ucore.bits.var == (mdev)->mcore.bits.var##_ack)
#define QMP_MCORE_CH_ACK_UPDATE(mdev, var) \
	do {(mdev)->mcore.bits.var##_ack = (mdev)->ucore.bits.var;} while(0)
#define QMP_MCORE_CH_VAR_ACK_CLR(mdev, var) \
	do {(mdev)->mcore.bits.var##_ack = 0;} while(0)

#define QMP_UCORE_CH_VAR_GET(mdev, var)	QMP_CH_VAR_GET(mdev, ucore, var)

#define QMP_UCORE_CH_ACKED_CHECK(mdev, var) \
	((mdev)->mcore.bits.var == (mdev)->ucore.bits.var##_ack)

#define QMP_UCORE_CH_VAR_TOGGLED_CHECK(mdev, var) \
	((mdev)->ucore.bits.var != (mdev)->mcore.bits.var##_ack)

/**
 * enum qmp_local_state - definition of the local state machine
 * @LINK_DISCONNECTED:		Init state, waiting for ucore to start
 * @LINK_NEGOTIATION:		Set local link state to up, wait for ucore ack
 * @LINK_CONNECTED:		Link state up, channel not connected
 * @LOCAL_CONNECTING:		Channel opening locally, wait for ucore ack
 * @LOCAL_CONNECTED:		Channel opened locally
 * @CHANNEL_CONNECTED:		Channel fully opened
 * @LOCAL_DISCONNECTING:	Channel closing locally, wait for ucore ack
 */
enum qmp_local_state {
	LINK_DISCONNECTED,
	LINK_NEGOTIATION,
	LINK_CONNECTED,
	LOCAL_CONNECTING,
	LOCAL_CONNECTED,
	CHANNEL_CONNECTED,
	LOCAL_DISCONNECTING,
};

union channel_desc {
	struct {
		u32 link_state:1;
		u32 link_state_ack:1;
		u32 ch_state:1;
		u32 ch_state_ack:1;
		u32 tx:1;
		u32 tx_ack:1;
		u32 rx_done:1;
		u32 rx_done_ack:1;
		u32 read_int:1;
		u32 read_int_ack:1;
		u32 reserved:6;
		u32 frag_size:8;
		u32 rem_frag_count:8;
	} bits;
	unsigned int val;
};

/**
 * struct qmp_device - local information for managing a single mailbox
 * @dev:		The device that corresponds to this mailbox
 * @ctrl:		The mbox controller for this mailbox
 * @mcore_desc:		Local core (APSS) mailbox descriptor
 * @ucore_desc:		Remote core (TME-L) mailbox descriptor
 * @mcore:		Local core (APSS) channel descriptor
 * @ucore:		Remote core (TME-L) channel descriptor
 * @mcore_mbox_size:	Local core mailbox size
 * @ucore_mbox_size:	Remote core mailbox size
 * @max_pkt_size:	Maximum supported packet size in Mailbox
 * @rx_pkt:		Buffer to pass to client, holds received data from mailbox
 * @tx_pkt:		Buffer from client, holds data to send on mailbox
 * @mbox_client:	Mailbox client for the IPC interrupt
 * @mbox_chan:		Mailbox client chan for the IPC interrupt
 * @local_state:	Current state of mailbox protocol
 * @state_lock:		Serialize mailbox state changes
 * @tx_lock:		Serialize access for writes to mailbox
 * @link_complete:	Use to block until link negotiation with remote proc
 * @ch_complete:	Use to block until the channel is fully opened
 * @dwork:		Delayed work to detect timed out tx
 * @tx_sent:		True if tx is sent and remote proc has not sent ack
 * @ch_in_use:		True if this mailbox's channel owned by a client
 * @rx_irq_line:	The incoming interrupt line
 * @tx_irq_count:	Number of tx interrupts triggered
 * @rx_irq_count:	Number of rx interrupts received
 */
struct qmp_device {
	struct device *dev;
	struct mbox_controller ctrl;

	void __iomem *mcore_desc;
	void __iomem *ucore_desc;
	union channel_desc mcore;
	union channel_desc ucore;
	size_t mcore_mbox_size;
	size_t ucore_mbox_size;
	u32 max_pkt_size;

	struct qmp_pkt rx_pkt;
	struct qmp_pkt tx_pkt;

	struct mbox_client mbox_client;
	struct mbox_chan *mbox_chan;

	enum qmp_local_state local_state;

	struct mutex state_lock;
	spinlock_t tx_lock;

	struct completion link_complete;
	struct completion ch_complete;
	struct delayed_work dwork;

	struct work_struct irq_work;

	bool tx_sent;
	bool ch_in_use;

	u32 rx_irq_line;
	u32 tx_irq_count;
	u32 rx_irq_count;
};

enum qmp_log_type {
	SEND_DATA,
	SEND_INTR,
	RX_WORK,
	RX_INTR,
	RX_DONE,
};

struct qmp_log {
	u64 timestamp;
	enum qmp_log_type type;
	union channel_desc mcore;
	union channel_desc ucore;
};

struct qmp_log qmp_log_buf[QMP_LOG_BUF_SIZE];
int qmp_log_index;
DEFINE_SPINLOCK(qmp_log_spinlock);

static void qmp_log_transaction(struct qmp_device *mdev, enum qmp_log_type type)
{
	unsigned long flags;

	spin_lock_irqsave(&qmp_log_spinlock, flags);

	qmp_log_buf[qmp_log_index].type = type;
	qmp_log_buf[qmp_log_index].mcore.val = mdev->mcore.val;
	qmp_log_buf[qmp_log_index].ucore.val = mdev->ucore.val;
	qmp_log_buf[qmp_log_index++].timestamp = ktime_to_ms(ktime_get());

	qmp_log_index &= (QMP_LOG_BUF_SIZE - 1);

	spin_unlock_irqrestore(&qmp_log_spinlock, flags);
}

/**
 * send_irq() - send an irq to a remote entity as an event signal.
 * @mdev:       Which remote entity that should receive the irq.
 */
static void send_irq(struct qmp_device *mdev)
{
	/* Update the mcore val to mcore register */
	iowrite32(mdev->mcore.val, mdev->mcore_desc);
	wmb();

	qmp_log_transaction(mdev, SEND_INTR);
	dev_dbg(mdev->dev, "%s: mcore 0x%x ucore 0x%x \n", __func__, mdev->mcore.val, mdev->ucore.val);

	mbox_send_message(mdev->mbox_chan, NULL);
	mbox_client_txdone(mdev->mbox_chan, 0);

        mdev->tx_irq_count++;
}

static void memcpy32_toio(void __iomem *dest, void *src, size_t size)
{
	u32 *dest_local = (u32 *)dest;
	u32 *src_local = (u32 *)src;

	WARN_ON(size & MBOX_ALIGN_BYTES);
	size /= sizeof(u32);
	while (size--)
		iowrite32(*src_local++, dest_local++);
}

static void memcpy32_fromio(void *dest, void __iomem *src, size_t size)
{
	u32 *dest_local = (u32 *)dest;
	u32 *src_local = (u32 *)src;

	WARN_ON(size & MBOX_ALIGN_BYTES);
	size /= sizeof(u32);
	while (size--)
		*dest_local++ = ioread32(src_local++);
}

/**
 * qmp_notify_timeout() - Notify client of tx timeout with -ETIME
 * @work:	Structure for work that was scheduled.
 */
static void qmp_notify_timeout(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct qmp_device *mdev = container_of(dwork, struct qmp_device, dwork);
	struct mbox_chan *chan = &mdev->ctrl.chans[0];
	int err = -ETIME;
	unsigned long flags;

	spin_lock_irqsave(&mdev->tx_lock, flags);
	if (!mdev->tx_sent) {
		spin_unlock_irqrestore(&mdev->tx_lock, flags);
		return;
	}
	mdev->tx_sent = false;
	spin_unlock_irqrestore(&mdev->tx_lock, flags);
	dev_dbg(mdev->dev, "%s: TX timeout\n",__func__);
	mbox_chan_txdone(chan, err);
}

static inline void qmp_schedule_tx_timeout(struct qmp_device *mdev)
{
	schedule_delayed_work(&mdev->dwork, msecs_to_jiffies(QMP_TX_TOUT_MS));
}

/**
 * qmp_startup() - Start qmp mailbox channel for communication. Waits for
 *			remote subsystem to open channel if link is not
 *			initated or until timeout.
 * @chan:	mailbox channel that is being opened.
 *
 * Return: 0 on succes or standard Linux error code.
 */
static int qmp_startup(struct mbox_chan *chan)
{
	struct qmp_device *mdev = chan->con_priv;
	int ret;

	if (!mdev)
		return -EINVAL;

	ret = wait_for_completion_timeout(&mdev->link_complete,
					  msecs_to_jiffies(QMP_TOUT_MS));
	if (!ret)
		return -EAGAIN;

	mutex_lock(&mdev->state_lock);
	if (mdev->local_state == LINK_CONNECTED) {
		QMP_MCORE_CH_VAR_SET(mdev, ch_state);
		mdev->local_state = LOCAL_CONNECTING;
		dev_dbg(mdev->dev, "link complete, local connecting\n");
		send_irq(mdev);
	}
	mutex_unlock(&mdev->state_lock);

	ret = wait_for_completion_timeout(&mdev->ch_complete,
					  msecs_to_jiffies(QMP_TOUT_MS));
	if (!ret)
		return -ETIME;

	return 0;
}

/**
 * qmp_send_data() - Copy the data to the channel's mailbox and notify
 *				remote subsystem of new data. This function will
 *				return an error if the previous message sent has
 *				not been read. Cannot Sleep.
 * @chan:	mailbox channel that data is to be sent over.
 * @data:	Data to be sent to remote processor, should be in the format of
 *		a qmp_pkt.
 *
 * Return: 0 on succes or standard Linux error code.
 */
static int qmp_send_data(struct mbox_chan *chan, void *data)
{
	struct qmp_device *mdev = chan->con_priv;
	struct qmp_pkt *pkt = (struct qmp_pkt *)data;
	void __iomem *addr;
	unsigned long flags;

	if (!mdev || !data || !completion_done(&mdev->ch_complete))
		return -EINVAL;

	if (pkt->size > mdev->max_pkt_size) {
		dev_err(mdev->dev, "Unsupported packet size %u\n",pkt->size);
		return -EINVAL;
	}

	spin_lock_irqsave(&mdev->tx_lock, flags);
	if (mdev->tx_sent) {
		spin_unlock_irqrestore(&mdev->tx_lock, flags);
		return -EAGAIN;
	}

	qmp_log_transaction(mdev, SEND_DATA);
	dev_dbg(mdev->dev, "%s: mcore 0x%x ucore 0x%x \n", __func__, mdev->mcore.val, mdev->ucore.val);

	addr = mdev->mcore_desc + QMP_CTRL_DATA_SIZE;
	memcpy32_toio(addr, pkt->data, pkt->size);

	mdev->mcore.bits.frag_size = pkt->size;
	mdev->mcore.bits.rem_frag_count = 0;

	dev_dbg(mdev->dev, "Copied buffer to mbox, sz: %d\n", mdev->mcore.bits.frag_size);

	mdev->tx_sent = true;
	QMP_MCORE_CH_VAR_TOGGLE(mdev, tx);
	send_irq(mdev);
	qmp_schedule_tx_timeout(mdev);
	spin_unlock_irqrestore(&mdev->tx_lock, flags);

	return 0;
}

/**
 * qmp_shutdown() - Disconnect this mailbox channel so the client does not
 *				receive anymore data and can reliquish control
 *				of the channel
 * @chan:	mailbox channel to be shutdown.
 */
static void qmp_shutdown(struct mbox_chan *chan)
{
	struct qmp_device *mdev = chan->con_priv;

	mutex_lock(&mdev->state_lock);

	if (mdev->local_state != LINK_DISCONNECTED) {
		mdev->local_state = LOCAL_DISCONNECTING;
		QMP_MCORE_CH_VAR_CLR(mdev, ch_state);
		send_irq(mdev);
	}
	mutex_unlock(&mdev->state_lock);
}

/**
 * qmp_last_tx_done() - qmp does not support polling operations, print
 *				error of unexpected usage and return true to
 *				resume operation.
 * @chan:	Corresponding mailbox channel for requested last tx.
 *
 * Return: true
 */
static bool qmp_last_tx_done(struct mbox_chan *chan)
{
	pr_err("In %s, unexpected usage of last_tx_done\n", __func__);
	return true;
}

/**
 * qmp_recv_data() - received notification that data is available in the
 *			mailbox. Copy data from mailbox and pass to client.
 * @mbox:		mailbox device that received the notification.
 * @mbox_of:	offset of mailbox after QMP Control data.
 */
static void qmp_recv_data(struct qmp_device *mdev, u32 mbox_of)
{
	void __iomem *addr;
	struct qmp_pkt *pkt;

	addr = mdev->ucore_desc + mbox_of;
	pkt = &mdev->rx_pkt;
	pkt->size = mdev->ucore.bits.frag_size;

	memcpy32_fromio(pkt->data, addr, mdev->ucore.bits.frag_size);

	QMP_MCORE_CH_ACK_UPDATE(mdev, tx);
	QMP_MCORE_CH_VAR_TOGGLE(mdev, rx_done);
	send_irq(mdev);

	dev_dbg(mdev->dev, "%s: Send RX data to TMEL Client\n", __func__);
	mbox_chan_received_data(&mdev->ctrl.chans[0], pkt);
}

/**
 * clr_mcore_ch_state() - Clear the mcore state of a mailbox.
 * @mdev:	mailbox device to be initialized.
 */
static void clr_mcore_ch_state(struct qmp_device *mdev)
{
	QMP_MCORE_CH_VAR_CLR(mdev, ch_state);
	QMP_MCORE_CH_VAR_ACK_CLR(mdev, ch_state);

	QMP_MCORE_CH_VAR_CLR(mdev, tx);
	QMP_MCORE_CH_VAR_ACK_CLR(mdev, tx);

	QMP_MCORE_CH_VAR_CLR(mdev, rx_done);
	QMP_MCORE_CH_VAR_ACK_CLR(mdev, rx_done);

	QMP_MCORE_CH_VAR_CLR(mdev, read_int);
	QMP_MCORE_CH_VAR_ACK_CLR(mdev, read_int);

	mdev->mcore.bits.frag_size = 0;
	mdev->mcore.bits.rem_frag_count = 0;
}

/**
 * qmp_irq_handler() - handle irq from remote entitity.
 * @irq:	irq number for the trggered interrupt.
 * @priv:	private pointer to qmp mbox device.
 */
static irqreturn_t qmp_irq_handler(int irq, void *priv)
{
	struct qmp_device *mdev = (struct qmp_device *)priv;

	mdev->rx_irq_count++;
	qmp_log_transaction(mdev, RX_INTR);

	queue_work(system_unbound_wq, &mdev->irq_work);
	return IRQ_HANDLED;
}

/**
 * __qmp_rx_worker() - Handle incoming messages from remote processor.
 * @mbox:	mailbox device that received notification.
 */
static void __qmp_rx_worker(struct qmp_device *mdev)
{
	unsigned long flags;

	qmp_log_transaction(mdev, RX_WORK);
	/* read remote_desc from mailbox register */
	mdev->ucore.val = ioread32(mdev->ucore_desc);

	dev_dbg(mdev->dev, "%s: mcore 0x%x ucore 0x%x \n", __func__, mdev->mcore.val, mdev->ucore.val);

	mutex_lock(&mdev->state_lock);

	/* Check if remote link down */
	if ((mdev->local_state >= LINK_CONNECTED) && (!QMP_UCORE_CH_VAR_GET(mdev, link_state))) {
		mdev->local_state = LINK_NEGOTIATION;
		QMP_MCORE_CH_ACK_UPDATE(mdev, link_state);
		send_irq(mdev);
		mutex_unlock(&mdev->state_lock);
		return;
	}

	switch (mdev->local_state) {
		case LINK_DISCONNECTED:
			QMP_MCORE_CH_VAR_SET(mdev, link_state);
			mdev->local_state = LINK_NEGOTIATION;
			mdev->rx_pkt.data = kzalloc(mdev->max_pkt_size, GFP_KERNEL);

			if (!mdev->rx_pkt.data) {
				dev_err(mdev->dev, "Failed to allocate rx pkt\n");
				break;
			}
			dev_dbg(mdev->dev, "Set to link negotiation\n");
			send_irq(mdev);

			break;
		case LINK_NEGOTIATION:
			if (!QMP_MCORE_CH_VAR_GET(mdev, link_state) ||
					!QMP_UCORE_CH_VAR_GET(mdev, link_state)) {
				dev_err(mdev->dev, "RX int without link state\n");
				break;
			}

			clr_mcore_ch_state(mdev);
			QMP_MCORE_CH_ACK_UPDATE(mdev, link_state);
			mdev->local_state = LINK_CONNECTED;
			complete_all(&mdev->link_complete);
			dev_dbg(mdev->dev, "Set to link connected\n");

			break;
		case LINK_CONNECTED:
			/* No need to handle until local opens */
			break;
		case LOCAL_CONNECTING:
			/* Ack to remote ch_state change */
			QMP_MCORE_CH_ACK_UPDATE(mdev, ch_state);

			mdev->local_state = CHANNEL_CONNECTED;
			complete_all(&mdev->ch_complete);
			dev_dbg(mdev->dev, "Set to channel connected\n");
			send_irq(mdev);
			break;
		case CHANNEL_CONNECTED:
			/* Check for remote channel down */
			if (!QMP_UCORE_CH_VAR_GET(mdev, ch_state)) {
				mdev->local_state = LOCAL_CONNECTING;
				QMP_MCORE_CH_ACK_UPDATE(mdev, ch_state);
				dev_dbg(mdev->dev, "Remote Disconnect\n");
				send_irq(mdev);
			}

			spin_lock_irqsave(&mdev->tx_lock, flags);
			/* Check TX done */
			if (mdev->tx_sent && QMP_UCORE_CH_VAR_TOGGLED_CHECK(mdev, rx_done)) {
				/* Ack to remote */
				QMP_MCORE_CH_ACK_UPDATE(mdev, rx_done);
				mdev->tx_sent = false;
				cancel_delayed_work(&mdev->dwork);
				dev_dbg(mdev->dev, "TX flag cleared\n");
				spin_unlock_irqrestore(&mdev->tx_lock, flags);
				mbox_chan_txdone(&mdev->ctrl.chans[0], 0);
				spin_lock_irqsave(&mdev->tx_lock, flags); /* needs changes */
			}
			spin_unlock_irqrestore(&mdev->tx_lock, flags);

			/* Check if remote is Transmitting */
			if (!QMP_UCORE_CH_VAR_TOGGLED_CHECK(mdev, tx))
				break;
			if ((mdev->ucore.bits.frag_size == 0) || (mdev->ucore.bits.frag_size >
						mdev->max_pkt_size)) {
				dev_err(mdev->dev, "Rx fragment size error %d\n", mdev->ucore.bits.frag_size);
				break;
			}
			qmp_recv_data(mdev, QMP_CTRL_DATA_SIZE);
			break;
		case LOCAL_DISCONNECTING:
			if (!QMP_MCORE_CH_VAR_GET(mdev, ch_state)) {
				clr_mcore_ch_state(mdev);
				mdev->local_state = LINK_CONNECTED;
				dev_dbg(mdev->dev, "Channel closed\n");
				reinit_completion(&mdev->ch_complete);
			}

			break;
		default:
			dev_err(mdev->dev, "Local Channel State corrupted\n");
	}
	mutex_unlock(&mdev->state_lock);
}

static void qmp_irq_work_handler(struct work_struct *work)
{
	struct qmp_device *mdev = container_of(work, struct qmp_device, irq_work);
	__qmp_rx_worker(mdev);
	qmp_log_transaction(mdev, RX_DONE);
}

/**
 * qmp_mbox_of_xlate() - Returns a mailbox channel to be used for this mailbox
 *			device. Make sure the channel is not already in use.
 * @mbox:	Mailbox device controlls the requested channel.
 * @spec:	Device tree arguments to specify which channel is requested.
 */
static struct mbox_chan *qmp_mbox_of_xlate(struct mbox_controller *mbox,
		const struct of_phandle_args *spec)
{
	struct qmp_device *mdev = dev_get_drvdata(mbox->dev);
	unsigned int channel = spec->args[0];

	if (!mdev)
		return ERR_PTR(-EPROBE_DEFER);

	if (channel >= mbox->num_chans)
		return ERR_PTR(-EINVAL);

	mutex_lock(&mdev->state_lock);
	if (mdev->ch_in_use) {
		dev_err(mdev->dev, "mbox channel already in use\n");
		mutex_unlock(&mdev->state_lock);
		return ERR_PTR(-EBUSY);
	}
	mdev->ch_in_use = true;
	mutex_unlock(&mdev->state_lock);

	return &mbox->chans[0];
}

static struct mbox_chan_ops qmp_mbox_ops = {
	.startup = qmp_startup,
	.shutdown = qmp_shutdown,
	.send_data = qmp_send_data,
	.last_tx_done = qmp_last_tx_done,
};

static int qmp_parse_devicetree(struct platform_device *pdev, struct qmp_device *mdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct resource *mbox_res;
	struct device *dev = &pdev->dev;
	int ret;

	mbox_res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mcore");
	if (!mbox_res) {
		dev_err(dev, "missing mcore reg base\n");
		return -EIO;
	}

	mdev->mcore_desc = devm_ioremap(dev, mbox_res->start, resource_size(mbox_res));
	if (!mdev->mcore_desc) {
		dev_err(dev, "ioremap failed for mcore reg\n");
		return -EIO;
	}
	mdev->mcore_mbox_size = resource_size(mbox_res);

	mbox_res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ucore");
	if (!mbox_res) {
		dev_err(dev, "missing ucore reg base\n");
		return -EIO;
	}

	mdev->ucore_desc = devm_ioremap(dev, mbox_res->start, resource_size(mbox_res));
	if (!mdev->ucore_desc) {
		dev_err(dev, "ioremap failed for ucore reg\n");
		return -EIO;
	}
	mdev->ucore_mbox_size = resource_size(mbox_res);

	ret = of_property_read_u32(node, "max_pkt_size", &mdev->max_pkt_size);
	if (ret) {
		dev_err(dev, "missing max supported packet size");
		return -EIO;
	}

	mdev->mbox_client.dev = dev;
	mdev->mbox_client.knows_txdone = true;
	mdev->mbox_chan = mbox_request_channel(&mdev->mbox_client, 0);
	if (IS_ERR(mdev->mbox_chan)) {
		dev_err(dev, "mbox chan for IPC is missing\n");
		return PTR_ERR(mdev->mbox_chan);
	}

	mdev->rx_irq_line = irq_of_parse_and_map(node, 0);
	if (!mdev->rx_irq_line) {
		dev_err(dev, "missing interrupt line\n");
		return -ENODEV;
	}

	return 0;
}

static int qmp_mbox_remove(struct platform_device *pdev)
{
	struct qmp_device *mdev = platform_get_drvdata(pdev);

	disable_irq(mdev->rx_irq_line);

	mbox_controller_unregister(&mdev->ctrl);
	kfree(mdev->rx_pkt.data);

	return 0;
}

static int qmp_mbox_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct mbox_chan *chans;
	struct qmp_device *mdev;
	int ret = 0;

	mdev = devm_kzalloc(&pdev->dev, sizeof(*mdev), GFP_KERNEL);
	if (!mdev)
		return -ENOMEM;
	platform_set_drvdata(pdev, mdev);

	ret = qmp_parse_devicetree(pdev, mdev);
	if (ret)
		return ret;

	mdev->dev = &pdev->dev;

	chans = devm_kzalloc(mdev->dev, sizeof(*chans) * QMP_NUM_CHANS, GFP_KERNEL);
	if (!chans)
		return -ENOMEM;

	mdev->ctrl.dev = &pdev->dev;
	mdev->ctrl.ops = &qmp_mbox_ops;
	mdev->ctrl.chans = chans;
	chans[0].con_priv = mdev;
	mdev->ctrl.num_chans = QMP_NUM_CHANS;
	mdev->ctrl.txdone_irq = true;
	mdev->ctrl.txdone_poll = false;
	mdev->ctrl.of_xlate = qmp_mbox_of_xlate;

	ret = mbox_controller_register(&mdev->ctrl);
	if (ret) {
		dev_err(mdev->dev, "failed to register mbox controller\n");
		return ret;
	}

	spin_lock_init(&mdev->tx_lock);
	mutex_init(&mdev->state_lock);
	mdev->local_state = LINK_DISCONNECTED;
	init_completion(&mdev->link_complete);
	init_completion(&mdev->ch_complete);

	INIT_DELAYED_WORK(&mdev->dwork, qmp_notify_timeout);
	INIT_WORK(&mdev->irq_work, qmp_irq_work_handler);

	mdev->tx_sent = false;
	mdev->ch_in_use = false;

	ret = devm_request_irq(mdev->dev, mdev->rx_irq_line,
				qmp_irq_handler,
				IRQF_TRIGGER_RISING,
				node->name, (void *)mdev);
	if (ret < 0) {
		dev_err(mdev->dev, "request threaded irq %d failed, ret %d\n",
			mdev->rx_irq_line, ret);
		qmp_mbox_remove(pdev);
		return ret;
	}

	ret = enable_irq_wake(mdev->rx_irq_line);
	if (ret < 0) {
		dev_err(mdev->dev, "enable_irq_wake on %d failed: %d\n",
			mdev->rx_irq_line, ret);
		qmp_mbox_remove(pdev);
		return ret;
	}

	/* Trigger fake RX in case of missed interrupt */
	if (of_property_read_bool(node, "qti,early-boot")) {
		__qmp_rx_worker(mdev);
	}

	dev_dbg(mdev->dev, "%s: completed\n",__func__);

	return 0;
}

static const struct of_device_id qmp_mbox_dt_match[] = {
	{ .compatible = "qti,tmel-qmp-mbox" },
	{},
};

static struct platform_driver qmp_mbox_driver = {
	.driver = {
		.name = "qmp_mbox",
		.of_match_table = qmp_mbox_dt_match,
	},
	.probe = qmp_mbox_probe,
	.remove = qmp_mbox_remove,
};

static int __init qmp_mbox_driver_init(void)
{
	return platform_driver_register(&qmp_mbox_driver);
}
arch_initcall(qmp_mbox_driver_init);

MODULE_DESCRIPTION("QTI Mailbox Protocol");
MODULE_LICENSE("GPL v2");
