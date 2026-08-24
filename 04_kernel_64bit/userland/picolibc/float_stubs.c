// Stubs for floating point and 64-bit math functions
// These override the Picolibc versions to avoid relocation errors

// Single precision float stubs
int __floatsisf(void) { return 0; }
int __floatunsisf(void) { return 0; }
int __fixsfsi(void) { return 0; }
int __fixunssfsi(void) { return 0; }
int __extendsfdf2(void) { return 0; }
int __truncdfsf2(void) { return 0; }
int __addsf3(int a, int b) { (void)a; (void)b; return 0; }
int __subsf3(int a, int b) { (void)a; (void)b; return 0; }
int __mulsf3(int a, int b) { (void)a; (void)b; return 0; }
int __divsf3(int a, int b) { (void)a; (void)b; return 0; }
int __eqsf2(int a, int b) { (void)a; (void)b; return 0; }
int __lesf2(int a, int b) { (void)a; (void)b; return 0; }
int __gtsf2(int a, int b) { (void)a; (void)b; return 0; }
int __nesf2(int a, int b) { (void)a; (void)b; return 0; }
int __ltsf2(int a, int b) { (void)a; (void)b; return 0; }
int __gesf2(int a, int b) { (void)a; (void)b; return 0; }

// Double precision float stubs
int __muldf3(int a, int b) { (void)a; (void)b; return 0; }
int __divdf3(int a, int b) { (void)a; (void)b; return 0; }
int __adddf3(int a, int b) { (void)a; (void)b; return 0; }
int __subdf3(int a, int b) { (void)a; (void)b; return 0; }
int __eqdf2(int a, int b) { (void)a; (void)b; return 0; }
int __ledf2(int a, int b) { (void)a; (void)b; return 0; }
int __gtdf2(int a, int b) { (void)a; (void)b; return 0; }
int __nedf2(int a, int b) { (void)a; (void)b; return 0; }
int __ltdf2(int a, int b) { (void)a; (void)b; return 0; }
int __gedf2(int a, int b) { (void)a; (void)b; return 0; }
int __fixdfsi(void) { return 0; }
int __fixunsdfsi(void) { return 0; }
int __floatsidf(void) { return 0; }
int __floatunsidf(void) { return 0; }

// 64-bit integer division stubs
int __divdi3(int a, int b) { (void)a; (void)b; return 0; }
int __udivdi3(int a, int b) { (void)a; (void)b; return 0; }
int __moddi3(int a, int b) { (void)a; (void)b; return 0; }
int __umoddi3(int a, int b) { (void)a; (void)b; return 0; }

// More stubs that might be needed
int __floatundisf(unsigned long long a) { (void)a; return 0; }
int __floatundidf(unsigned long long a) { (void)a; return 0; }
int __floatdidf(long long a) { (void)a; return 0; }
int __floatdisf(long long a) { (void)a; return 0; }
