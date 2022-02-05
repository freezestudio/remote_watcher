# dll 类型的 Windows 服务

不同于通常的控制台应用程序类型的 Windows 服务, 我们创建 dll 类型的例程, 使用 svchost.exe 来加载.

## 注册表项

服务

```shell
HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\SERVICE_NAME:
#   DependOnService REG_MULTI_SZ  xxx yyy ...
  Description        REG_SZ        服务说明
  DisplayName        REG_SZ        SERVICE_NAME
#   ErrorControl       REG_DWORD     0x00000001
#   FailureActions     REG_BINARY    80 51 01 00 ...
  ImagePath          REG_EXPAND_SZ %SystemRoot%\system32\svchost.exe -k LocalService -p
  ObjectName         REG_SZ        LocalSystem
#   RequiredPrivileges REG_MULTI_SZ  SelmpersonatePrivilege SeCreateGlobalPrivilege
  Start              REG_DWORD     0x00000002 # 1:延迟 2:自动 3:手动 4:禁用
  Type               REG_WORD      0x00000010

  \Parameters:
    ServiceDll             REG_SZ        full\path\to\xxx\yyy.dll
    ServiceDllUnloadOnStop REG_DWORD     0x00000001
    ServiceMain            REG_SZ        ServiceMain
```

SvcHost 组

```
HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Svchost:
  LocalService             REG_MULTI_SZ      nsi WdiServiceHost ... SERVICE_NAME
```


## dll 及相关文件的释放位置

1. 所有文件使用 zip 打包
2. 制作资源 dll

## 概念

* 服务应用程序: 符合(SCM)接口规则
* 驱动程序服务: 不与(SCM)交互
* 服务控制管理器
* 服务数据库 `HKEY_LOCAL_MACHINE_SYSTEM\CurrentControlSet\Services`
* 服务
* 服务计划
* 服务配置
* 服务控制

## 其它

* 线程池与 APC
