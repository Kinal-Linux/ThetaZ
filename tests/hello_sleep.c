// ThetaZ sleep test: keeps the host alive on the main thread so an external
// IPC client can connect to the θserver socket while the guest is running.
#include <windows.h>

int main(void) {
    Sleep(2500);
    return 0;
}
