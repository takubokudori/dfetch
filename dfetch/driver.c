/* プロジェクトの設定で最適化を無効 (/Od) にしている。
 * 無効にしないとWhoopsのパスごと最適化で削除される。
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

// NEITHER I/Oにすることで入力バッファをユーザランドメモリ領域にする
// METHOD_BUFFERとかにするとカーネルランドのメモリにバッファリングされたものを参照してしまうのでダメ
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

// 特に何もしない
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
        // もしMETHOD_BUFFERを使用していた場合は以下からメモリを参照することになるが、
        // これはカーネルアドレスにバッファされている領域を参照するのでdouble fetchはできない。
        // PVOID inputBuffer = Irp->AssociatedIrp.SystemBuffer;
        switch (ioControlCode)
        {
        case IOCTL_DOUBLE_FETCH:
            // 読み取り可能か先に検証
            ProbeForRead(inputBuffer, sizeof(DWORD), TYPE_ALIGNMENT(DWORD));
            // DbgPrint("HELLO %p", inputBuffer);
            // first fetch
            // ここで==5であることを検証
            if (*(DWORD*)inputBuffer != 5) {
                // !=5だった場合はエラーにするだけ
                DbgPrint("Error!");
                goto DONE;
            }
            // この時点で*(DWORD*)inputBuffer==5であることを検証済み

#if 0 
            // 無駄な処理を挟むと成功確率が上がる
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
            // 上のif文で検証してからここに到達するまでの間にinputBufferの中身が変化していればぬるぽ参照を起こす。
            if (*(DWORD*)inputBuffer != 5)
            {
                // この時点で!=5だった場合はクラッシュさせる。
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

