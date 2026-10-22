// ThetaZ test DLL: two pure exports, no CRT data/TLS.
__declspec(dllexport) int add(int a, int b) { return a + b; }
__declspec(dllexport) int sub(int a, int b) { return a - b; }
