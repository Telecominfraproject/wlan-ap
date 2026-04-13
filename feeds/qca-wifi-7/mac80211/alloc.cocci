@@
expression E1, E2;
identifier p,p1;
@@

struct p1 *p;
...
-p = kmalloc(E1,E2);
+p = kmalloc(E1,E2,p1);

@@
expression E1, E2;
identifier p,p1;
@@
-struct p1 *p = kzalloc(E1,E2);
+struct p1 *p = kzalloc(E1,E2,p1);


@kzalloc_present@
expression size, flags;
identifier k,p,p1;
@@
k = kzalloc(size, flags);

@transform depends on kzalloc_present@
expression size, flags;
identifier p1, p;
@@
struct p1 *p;
... when exists
- p = kzalloc(size, flags);
+ p = kzalloc(size, flags, p1);


@kzalloc_present1@
expression size, flags;
identifier k;
@@
k = kzalloc(size, flags);

@@
expression E1, E2;
identifier p,p1;
@@

struct p1 *p;
... when exists
if (...) {
-p = kzalloc(E1,E2);
+p = kzalloc(E1,E2,p1);
}


@kzalloc_present2@
expression size, flags;
identifier k,p,p1;
@@
k = kzalloc(size, flags);

@@
expression E1, E2;
identifier p,p1;
@@

struct p1 *p;
... when exists
while (...) {
-p = kzalloc(E1,E2);
+p = kzalloc(E1,E2,p1);
}


@@
expression E1, E2;
identifier p,p1,q;
@@
struct p1 *p;
... when exists
-p = (struct p1 *) kzalloc(E1,E2);
+p = (struct p1 *) kzalloc(E1,E2,p1);

