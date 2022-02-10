# 主要代码执行流程

## 启动服务

## 停止服务

## 接收消息

```shell
# 1. _nats_connect::on_recv_message(msg);
# 2. _SleepThread() wakeup;
# 3. maybe_send_message();
# 4. nats_client::notify_message();
# 5. _SleepThread() sleep;
# 6. nats_client->message-thread wakeup;
# 7. nats_client::message_response();
# 8. nats_client->message-thread waiting;
```

## 接收命令

```shell
# 1. _nats_connect::on_command(cmd);
# 2. _SleepThread() wakeup;
# 3. maybe_response_command();
# 4. nats_client::notify_command();
# 5. nats_client->command-thread wakeup;
# 6. nats_client::command_handle_result();
# 7. nats_client->command-thread waiting;
# 8. (optional) _WorkThread() wakeup -> _WorkThread() sleep;
# 9. _SleepThread() sleep;
```
## 发送文件

```shell
# 1. watcher_win32::start();
# 2. folder_watchor_base::start();
# 3. folder_watchor_base::watch();
# 4. folder_watchor_base::thread loop, send payload;
# 5. _SleepThread() wakeup;
# 6. maybe_send_payload();
# 7. nats_client::notify_payload();
# 8. nats_client::payload-thread wakeup;
# 9. nats_client::send_payload(); -> publish_payload();
# 10. nats_client::payload-thread sleep;
# 11. _SleepThread() sleep;
```
