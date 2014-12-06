UTF8
--------
※作業中で、動作しません。

dds_thumbnail.dll
Windows Vista以降のPCでddsファイルのサムネイル表示を行います。

--------
このプログラムは以下のサンプルコードを参考に制作しました。
https://code.msdn.microsoft.com/windowsapps/CppShellExtThumbnailHandler-32399b35

--------
使い方.

1. 任意の設定でビルドを行います。64bit Windowsの場合はx64設定でビルドしてください。

2. 管理者権限を持ったコマンドプロンプトで、以下のコマンドを実行します。
Regsvr32.exe dds_thumbnail.dll

3. 停止する場合は同様に以下のコマンドを実行します。
Regsvr32.exe /u dds_thumbnail.dll
