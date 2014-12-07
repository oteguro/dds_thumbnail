UTF8
--------
※開発中の為、.ddsファイルを.hogeを拡張子を変えたもののみ動作します。
  まだ十分にテストされていません。

dds_thumbnail.dll
Windows Vista以降のエクスプローラーでddsファイルのサムネイル表示を行います。
BC4以降のサムネイル表示を行うソフトがあまりなかったので作ってみました。
DDSの扱いはDirectXTexにお任せしています。

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
