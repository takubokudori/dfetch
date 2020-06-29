/* �v���W�F�N�g�̐ݒ�ōœK���𖳌� (/Od) �ɂ��Ă���B
 * �����ɂ��Ȃ���Whoops�̃p�X���ƍœK���ō폜�����B
 *
 */
#include <wdm.h>
#include <windef.h>
#pragma warning(disable: 4100)
#pragma warning(disable: 4116)

DRIVER_INITIALIZE DriverEntry;
DRIVER_UNLOAD DriverUnload;
DRIVER_DISPATCH UnsupportedHandler;
DRIVER_DISPATCH VulnIoctlHandler;

// NEITHER I/O�ɂ��邱�Ƃœ��̓o�b�t�@�����[�U�����h�������̈�ɂ���
// METHOD_BUFFER�Ƃ��ɂ���ƃJ�[�l�������h�̃������Ƀo�b�t�@�����O���ꂽ���̂��Q�Ƃ��Ă��܂��̂Ń_��
#define IOCTL_DOUBLE_FETCH CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_NEITHER,FILE_ANY_ACCESS)

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    UNICODE_STRING DeviceName, DosDeviceName;
    PDEVICE_OBJECT DeviceObject;

    RtlInitUnicodeString(&DeviceName, L"\\Device\\DFetchBug");
    RtlInitUnicodeString(&DosDeviceName, L"\\DosDevices\\DFetchBug");

    IoCreateDevice(DriverObject, 0, &DeviceName, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, FALSE, &DeviceObject);
    IoCreateSymbolicLink(&DosDeviceName, &DeviceName);

    DbgPrint("DFetchBug driver installed!");
    for (int i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++) {
        DriverObject->MajorFunction[i] = UnsupportedHandler;
    }

    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = VulnIoctlHandler;

    DriverObject->DriverUnload = DriverUnload;
    return STATUS_SUCCESS;
}

VOID DriverUnload(PDRIVER_OBJECT DriverObject)
{
    UNICODE_STRING DosDeviceName;
    RtlInitUnicodeString(&DosDeviceName, L"\\DosDevices\\DFetchBug");
    IoDeleteSymbolicLink(&DosDeviceName);
    IoDeleteDevice(DriverObject->DeviceObject);
}

// ���ɉ������Ȃ�
NTSTATUS UnsupportedHandler(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    return STATUS_NOT_SUPPORTED;
}

NTSTATUS VulnIoctlHandler(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PIO_STACK_LOCATION pIoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    DWORD ioControlCode = pIoStackLocation->Parameters.DeviceIoControl.IoControlCode;
    PVOID inputBuffer = pIoStackLocation->Parameters.DeviceIoControl.Type3InputBuffer;

    __try
    {
        // ����METHOD_BUFFER���g�p���Ă����ꍇ�͈ȉ����烁�������Q�Ƃ��邱�ƂɂȂ邪�A
        // ����̓J�[�l���A�h���X�Ƀo�b�t�@����Ă���̈���Q�Ƃ���̂�double fetch�͂ł��Ȃ��B
        // PVOID inputBuffer = Irp->AssociatedIrp.SystemBuffer;
        switch (ioControlCode)
        {
        case IOCTL_DOUBLE_FETCH:
            // �ǂݎ��\����Ɍ���
            ProbeForRead(inputBuffer, sizeof(DWORD), TYPE_ALIGNMENT(DWORD));
            // DbgPrint("HELLO %p", inputBuffer);
            // first fetch
            // ������==5�ł��邱�Ƃ�����
            if (*(DWORD*)inputBuffer != 5) {
                // !=5�������ꍇ�̓G���[�ɂ��邾��
                DbgPrint("Error!");
                goto DONE;
            }
            // ���̎��_��*(DWORD*)inputBuffer==5�ł��邱�Ƃ����؍ς�

#if 0 
            // ���ʂȏ��������ނƐ����m�����オ��
            int i = 0;
            for (i = 0; i < 20000; i++)
            {
                i += 3;
                i -= 3;
                i += 4;
                i -= 4;
            }
#endif
            // double fetch
            // ���if���Ō��؂��Ă��炱���ɓ��B����܂ł̊Ԃ�inputBuffer�̒��g���ω����Ă���΂ʂ�ێQ�Ƃ��N�����B
            if (*(DWORD*)inputBuffer != 5)
            {
                // ���̎��_��!=5�������ꍇ�̓N���b�V��������B
                DbgPrint("Whooooooops!!");
                KeBugCheckEx(APC_INDEX_MISMATCH, 0, 0, 0, 0);
            }
            break;

        default:
            DbgPrint("Unknown ioControlCode!");
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        DbgPrint("ProbeForRead Error!");
        goto DONE;
    }


DONE:
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Irp->IoStatus.Status;
}

