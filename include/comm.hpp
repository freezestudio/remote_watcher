#pragma once

// user-defined service control range 128~255
#define SERVICE_CONTROL_NETWORK_CONNECT 129 
#define SERVICE_CONTROL_NETWORK_DISCONNECT 130

namespace freeze
{
    class service
    {
    public:
        service(SC_HANDLE h = nullptr)
            : _sc_handle{h}, _ss_handle{nullptr}
        {
            // OpenSCManager(nullptr,nullptr,SC_MANAGER_ALL_ACCESS);
        }

        explicit service(SC_HANDLE hsc, SERVICE_STATUS_HANDLE hss)
            : _sc_handle{hsc}
            , _ss_handle{hss}
        {

        }

        ~service()
        {
            // if(_ss_handle)
            // {
            //     CloseServiceHandle(_ss_handle);
            // }
            if(_sc_handle)
            {
                CloseServiceHandle(_sc_handle);
            }
        }

        explicit /* constexpr */ operator bool() const
        {
            return !!_sc_handle;
        }

    public:
        static DWORD __stdcall controll_handler(
            DWORD dwControl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext)
        {
            // dwEventType:
            // The type of event that has occurred.
            // This parameter is used if dwControl is
            //   SERVICE_CONTROL_DEVICEEVENT,
            //   SERVICE_CONTROL_HARDWAREPROFILECHANGE,
            //   SERVICE_CONTROL_POWEREVENT,
            //   or SERVICE_CONTROL_SESSIONCHANGE.
            // Otherwise, it is zero.

            // lpContext
            //   user-defined data from RegisterServiceCtrlHandlerEx

            auto pservice = reinterpret_cast<service *>(lpContext);

            switch (dwControl)
            {
            case SERVICE_CONTROL_PAUSE:
                // pause service
                pservice->update_state(SERVICE_PAUSE_PENDING);
                break;
            case SERVICE_CONTROL_CONTINUE:
                // resume service
                pservice->update_state(SERVICE_CONTINUE_PENDING);
                break;
            case SERVICE_CONTROL_INTERROGATE:
                // report current status, should simply return NO_ERROR
                break;
            case SERVICE_CONTROL_PARAMCHANGE:
                // startup parameters changed
                break;
            // case SERVICE_CONTROL_PRESHUTDOWN:
            // pre-shutdown
            // break;
            case SERVICE_CONTROL_SHUTDOWN:
                // shutdown, return NO_ERROR;
                break;
            case SERVICE_CONTROL_STOP:
                // stop service, eturn NO_ERROR;
                pservice->update_state(SERVICE_STOP_PENDING);
                break;
            case SERVICE_CONTROL_NETWORK_CONNECT:
                // return ERROR_CALL_NOT_IMPLEMENTED;
                break;
            case SERVICE_CONTROL_NETWORK_DISCONNECT:
                // return ERROR_CALL_NOT_IMPLEMENTED;
                break;
            default:
                break;
            }
            return NO_ERROR;
        }

        static void __stdcall entry(DWORD argc, LPCWSTR *argv)
        {
        }

    public:
        bool create(std::wstring const &name)
        {
            return false;
        }

        void set_display_name(std::wstring const &name)
        {
            ChangeServiceConfig(
                _sc_handle,
                SERVICE_NO_CHANGE,
                SERVICE_NO_CHANGE,
                SERVICE_NO_CHANGE,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                name.data()
            );
        }

        void set_descripation(std::wstring const &desc)
        {
            SERVICE_DESCRIPTION sdesc{
                .lpDescription=desc.c_str()
            };
	        ChangeServiceConfig2(sc, SERVICE_CONFIG_DESCRIPTION, &sdesc);
        }

        void set_binary_path(fs::path const &bin)
        {
        }

        void set_security_identifier(DWORD sid = SERVICE_SID_TYPE_NONE)
        {
            // SERVICE_SID_TYPE_NONE
            // SERVICE_SID_TYPE_RESTRICTED
            // SERVICE_SID_TYPE_UNRESTRICTED
            SERVICE_SID_INFO ssi {
                .dwServiceSidType = sid,
            };
            ChangeServiceConfig2(_sc_handle, SERVICE_CONFIG_SERVICE_SID_INFO, &ssi);
        }

        // control
    public:
        bool pause()
        {
            SERVICE_STATUS status;
            auto ok = ControlService(_sc_handle, SERVICE_CONTROL_PAUSE, &status) ? true : false;
            // query status
            return ok;
        }

        bool resume()
        {
            SERVICE_STATUS status;
            auto ok = ControlService(_sc_handle, SERVICE_CONTROL_CONTINUE, &status) ? true : false;
            // query status
            return ok;
        }

        bool stop()
        {
            SERVICE_STATUS status;
            auto ok = ControlService(_sc_handle, SERVICE_CONTROL_CONTINUE, &status) ? true : false;
            // query status
            return ok;
        }

        bool network_connect()
        {
            SERVICE_STATUS status;
            auto ok = ControlService(_sc_handle, SERVICE_CONTROL_NETWORK_CONNECT, &status) ? true : false;
            // query status
            return ok;
        }

        bool network_disconnect()
        {
            SERVICE_STATUS status;
            auto ok = ControlService(_sc_handle, SERVICE_CONTROL_NETWORK_DISCONNECT, &status) ? true : false;
            // query status
            return ok;
        }

    public:
        bool update_state(DWORD state, DWORD error_code = NO_ERROR)
        {
            if(!_ss_handle)
            {
                return false;
            }

            SERVICE_STATUS service_status {
                .dwServiceType = SERVICE_WIN32_OWN_PROCESS,
                .dwServiceSpecificExitCode = 0, // ignore, unless dwWin32ExitCode=ERROR_SERVICE_SPECIFIC_ERROR
            };

            // dwCurrentStatus:
            //   SERVICE_CONTINUE_PENDING 0x00000005 The service continue is pending.
            //   SERVICE_PAUSE_PENDING    0x00000006 The service pause is pending.
            //   SERVICE_PAUSED           0x00000007 The service is paused.
            //   SERVICE_RUNNING          0x00000004 The service is running.
            //   SERVICE_START_PENDING    0x00000002 The service is starting.
            //   SERVICE_STOP_PENDING     0x00000003 The service is stopping.
            //   SERVICE_STOPPED          0x00000001 The service is not running.

            // dwWaitHint:
            // The estimated time required for
            // a pending start, stop, pause, or continue operation, in milliseconds.
            // Before the specified amount of time has elapsed,
            // the service should make its next call to the SetServiceStatus function
            // with either an incremented dwCheckPoint value or a change in dwCurrentState.
            // If the amount of time specified by dwWaitHint passes,
            // and dwCheckPoint has not been incremented or dwCurrentState has not changed,
            // the service control manager or service control program
            // can assume that an error has occurred and the service should be stopped.
            // However, if the service shares a process with other services,
            // the service control manager cannot terminate the service application
            // because it would have to terminate the other services sharing the process as well.

            // best practices:
            // Initialize all fields in the SERVICE_STATUS structure,
            // ensuring that there are valid check-point and wait hint values for pending states.
            // Use reasonable wait hints.
            //
            // Do not register to accept controls while the status is SERVICE_START_PENDING
            // or the service can crash. After initialization is completed,
            // accept the SERVICE_CONTROL_STOP code.
            //
            // Call this function with checkpoint and wait-hint values
            // only if the service is making progress on the tasks related to
            // the pending start, stop, pause, or continue operation.
            // Otherwise, SCM cannot detect if your service is hung.
            //
            // Enter the stopped state with an appropriate exit code if ServiceMain fails.
            //
            // If the status is SERVICE_STOPPED,
            // perform all necessary cleanup and call SetServiceStatus one time only.
            // This function makes an LRPC call to the SCM.
            // The first call to the function in the SERVICE_STOPPED state
            // closes the RPC context handle and any subsequent calls can cause the process to crash.
            //
            // Do not attempt to perform any additional work
            // after calling SetServiceStatus with SERVICE_STOPPED,
            // because the service process can be terminated at any time.

            service_status.dwCurrentState = state;
            service_status.dwWin32ExitCode = error_code;
            if (service_status.dwCurrentState == SERVICE_START_PENDING)
            {
                service_status.dwControlsAccepted = 0;
            }
            else
            {
                service_status.dwControlsAccepted =
                    SERVICE_ACCEPT_PARAMCHANGE |
                    SERVICE_ACCEPT_PAUSE_CONTINUE |
                    SERVICE_ACCEPT_SHUTDOWN |
                    SERVICE_ACCEPT_STOP;
            }

            if (service_status.dwCurrentState == SERVICE_CONTINUE_PENDING ||
                service_status.dwCurrentState == SERVICE_PAUSE_PENDING ||
                service_status.dwCurrentState == SERVICE_START_PENDING ||
                service_status.dwCurrentState == SERVICE_STOP_PENDING)
            {
                service_status.dwCheckPoint = ++_check_point;
                service_status.dwWaitHint = 3000;
            }
            else
            {
                service_status.dwCheckPoint = 0;
                service_status.dwWaitHint = 0;
            }

            auto ok = SetServiceStatus(_ss_handle, &service_status) : true : false;
            return ok;
        }

        void do_pause()
        {
            // ...
            update_state(SERVICE_PAUSED);
        }

        void do_resume()
        {
            // ...
            update_state(SERVICE_RUNNING);
        }

        // void do_start()
        // {
        // }

        void do_stop()
        {
            // ...
            update_state(SERVICE_STOPPED);
        }

        void do_network_connect()
        {

        }

        void do_network_disconnect()
        {

        }

    public:
        bool notify_status()
        {
            // SERVICE_NOTIFY_2 {
            //     DWORD                  dwVersion;
            //     PFN_SC_NOTIFY_CALLBACK pfnNotifyCallback;
            //     PVOID                  pContext;
            //     DWORD                  dwNotificationStatus;
            //     SERVICE_STATUS_PROCESS ServiceStatus;
            //     DWORD                  dwNotificationTriggered;
            //     LPWSTR                 pszServiceNames;
            // }
            SERVICE_NOTIFY_2 notify2{
                .dwVersion = SERVICE_NOTIFY_STATUS_CHANGE,
                .dwNotificationStatus = ERROR_SUCCESS,
            };

            // If the function succeeds, the return value is ERROR_SUCCESS.
            //
            // If the service has been marked for deletion,
            // the return value is ERROR_SERVICE_MARKED_FOR_DELETE and the handle to the service must be closed.
            //
            // If service notification is lagging too far behind the system state,
            // the function returns ERROR_SERVICE_NOTIFY_CLIENT_LAGGING.
            // In this case, the client should close the handle to the SCM,
            // open a new handle, and call this function again.
            auto ret = NotifyServiceStatusChange(
                _sc_handle,
                SERVICE_NOTIFY_CREATED |
                SERVICE_NOTIFY_CONTINUE_PENDING |
                SERVICE_NOTIFY_DELETE_PENDING |
                SERVICE_NOTIFY_DELETED |
                SERVICE_NOTIFY_PAUSE_PENDING |
                SERVICE_NOTIFY_PAUSED |
                SERVICE_NOTIFY_RUNNING |
                SERVICE_NOTIFY_START_PENDING |
                SERVICE_NOTIFY_STOP_PENDING |
                SERVICE_NOTIFY_STOPPED,
                &notify2
            );

            return ret == ERROR_SUCCESS;
        }

    private:
        bool query_status(SERVICE_STATUS &service_status)
        {
            BOOL ret = FALSE;
            uint8_t *buffer = nullptr;
            DWORD buffer_size = 0;
            DWORD bytes_needed = 0;
            do
            {
                ret = QueryServiceStatusEx(
                    _sc_handle, SC_STATUS_PROCESS_INFO, buffer, buffer_size, &bytes_needed);
                if (!ret)
                {
                    auto err = GetLastError();
                    if (err == ERROR_INSUFFICIENT_BUFFER)
                    {
                        buffer_size = bytes_needed;
                        if (buffer)
                        {
                            delete[] buffer;
                            buffer = nullptr;
                        }
                        buffer = new uint8_t[buffer_size]{};
                        ret = QueryServiceStatusEx(
                            _sc_handle, SC_STATUS_PROCESS_INFO,
                            buffer, buffer_size, &bytes_needed);
                    }
                    else
                    {
                        return false;
                    }
                }
            } while (!ret);

            auto pstatus = reinterpret_cast<LPSERVICE_STATUS_PROCESS>(buffer);
            service_status.dwCheckPoint = pstatus->dwCheckPoint;
            service_status.dwControlsAccepted = pstatus->dwControlsAccepted;
            service_status.dwCurrentState = pstatus->dwCurrentState;
            service_status.dwServiceSpecificExitCode = pstatus->dwServiceSpecificExitCode;
            service_status.dwServiceType = pstatus->dwServiceType;
            service_status.dwWaitHint = pstatus->dwWaitHint;
            service_status.dwWin32ExitCode = pstatus->dwWin32ExitCode;

            delete[] buffer;
            return true;
        }

    private:
        SC_HANDLE _sc_handle;
        SERVICE_STATUS_HANDLE _ss_handle;
        DWORD _check_point = 0;
    };
}

namespace freeze
{
    class service_manager
    {
    public:
        service_manager()
        {
            _scm_handle = ::OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
        }

        ~service_manager()
        {
            CloseServiceHandle(_scm_handle);
        }

        service_manager(service_manager const &) = delete;
        service_manager &operator=(service_manager const &) = delete;

        service_manager(service_manager &&rhs)
            : _scm_handle{std::exchange(rhs._scm_handle, nullptr)}
        {
        }

        service_manager &operator=(service_manager &&rhs)
        {
            _scm_handle = std::exchange(rhs._scm_handle, nullptr);
            return *this;
        }

    public:
        std::vector<std::wstring> enum_win32_services()
        {
            std::vector<std::wstring> vec_services;
            BOOL ret = FALSE;
            // ENUM_SERVICE_STATUS_PROCESS;
            uint8_t *services = nullptr;
            DWORD bytes_count = 0;
            DWORD bytes_needlen = 0;
            DWORD service_count = 0;
            DWORD resume = 0;
            do
            {
                ret = ::EnumServicesStatusEx(
                    _scm_handle,
                    SC_ENUM_PROCESS_INFO,
                    SERVICE_WIN32,
                    SERVICE_STATE_ALL,
                    (LPBYTE)services,
                    bytes_count,
                    &bytes_needlen,
                    &service_count,
                    &resume,
                    nullptr);
                if (!ret)
                {
                    ERROR_MORE_DATA;
                    auto err = GetLastError();
                    if (err == ERROR_MORE_DATA)
                    {
                        if (bytes_needlen > 0)
                        {
                            bytes_count = bytes_needlen;
                            if (services)
                            {
                                delete[] services;
                                services = nullptr;
                            }
                            services = new uint8_t[bytes_count]{};
                        }
                    }
                    else
                    {
                        /* error code */
                    }
                }
                else
                {
                    auto ssp = reinterpret_cast<ENUM_SERVICE_STATUS_PROCESS *>(services);
                    for (int i = 0; i < static_cast<int>(service_count); ++i)
                    {
                        vec_services.push_back(ssp[i].lpServiceName);
                    }
                    if (services)
                    {
                        delete[] services;
                        services = nullptr;
                    }
                }

            } while (!ret);
            return vec_services;
        }

        bool exists(std::wstring const &name, bool ignore_case = true)
        {

            using namespace std::ranges;
            using namespace std::views;
            auto services = enum_win32_services();

            auto _end = std::end(services);
            if (ignore_case)
            {
                auto finded = std::find_if(std::begin(services), _end, [&name](auto &&v)
                                           {
                                               std::wstring _lname;
                                               std::transform(std::cbegin(name), std::cend(name), std::back_inserter(_lname), ::tolower);
                                               std::wstring _lv;
                                               std::transform(std::cbegin(v), std::cend(v), std::back_inserter(_lv), ::tolower);
                                               return _lname == _lv;
                                           });
                return finded != _end;
            }
            auto finded = std::find(std::begin(services), std::end(services), name);
            return finded != _end;

            // if (ignore_case)
            // {
            //     auto _c = services |
            //               std::views::transform(::tolower) |
            //               std::views::filter([&name](auto &&v)
            //                                  {
            //                                      auto lower_name = std::transform(std::begin(name), std::end(name), ::tolower);
            //                                      return *lower_name == v;
            //                                  });
            //     for (auto &x : _c)
            //     {
            //         return true;
            //     }
            //     return false;
            // }
            // else
            // {
            //     auto _c = services |
            //               std::views::filter([&name](auto &&v)
            //                                  { return name == v; });
            //     for (auto &x : _c)
            //     {
            //         return true;
            //     }
            //     return false;
            // }
        }

        service get_service(std::wstring const &name)
        {
            if (!exists(name))
            {
                return nullptr;
            }
        }

        service create(std::wstring const &name)
        {
            return nullptr;
        }

        service open(std::wstring const &name)
        {
            return nullptr;
        }

        bool install_service(std::wstring const &name)
        {
            return false;
        }

        bool uninstall_service(std::wstring const &name)
        {
            return false;
        }

        bool start_service(std::wstring const &name, std::wstring const &ip)
        {
            return false;
        }

        bool stop_service(std::wstring const &name)
        {
            return false;
        }

    private:
        SC_HANDLE _scm_handle;
    };
}
