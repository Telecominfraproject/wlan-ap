{%
if (!fs.stat("/sys/fs/pstore/dmesg-ramoops-0"))
	return 0;
let fd = fs.open("/sys/fs/pstore/dmesg-ramoops-0", "r");
let line, lines = [];
while (line = fd.read("line"))
	push(lines, trim(line));
fd.close();
let fd = fs.open("/tmp/crashlog", "w");
fd.write({crashlog: lines});
fd.close();
print(lines);
%}
