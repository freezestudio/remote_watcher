# compress

```shell
# minizip -h

Usage : minizip [-o] [-a] [-0 to -9] [-p password] [-j] file.zip [files_to_add]

  -o  Overwrite existing file.zip
  -a  Append to existing file.zip
  -0  Store only
  -1  Compress faster
  -9  Compress better

  -j  exclude path. store only the file name.
```

```shell
# miniunz

Usage : miniunz [-e] [-x] [-v] [-l] [-o] [-p password] file.zip [file_to_extr.] [-d extractdir]

  -e  Extract without pathname (junk paths)
  -x  Extract with pathname
  -v  list files
  -l  list files
  -d  directory to extract into
  -o  overwrite files without prompting
  -p  extract crypted file using password
```