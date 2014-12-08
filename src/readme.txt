UTF8
--------
※十分にテストされていないのでまだまだバグがあるかも。

dds_thumbnail.dll
Windows Vista以降のエクスプローラーでddsファイルのサムネイル表示を行います。
BC4以降のサムネイル表示を行うソフトがあまりなかったので作ってみました。
DDSの扱いはDirectXTexにお任せしています。

BC1～BC7, ARGB8, ABGR16F, ABGRF32, RG16F RG8, L8 などはWindows8.1上で動作確認しました。

※対応されていないフォーマットは黒いアイコンになります。

--------
このプログラムは以下のサンプルコードを参考に制作しました。（ほぼそのままです）
https://code.msdn.microsoft.com/windowsapps/CppShellExtThumbnailHandler-32399b35

--------
使い方.

1. 任意の設定でビルドを行います。64bit Windowsの場合はx64設定でビルドしてください。

2. 管理者権限を持ったコマンドプロンプトで、以下のコマンドを実行します。
Regsvr32.exe dds_thumbnail.dll

3. 停止する場合は同様に以下のコマンドを実行します。
Regsvr32.exe /u dds_thumbnail.dll

--------
残作業

・マイナーフォーマットの確認.
・パフォーマンスに関する考慮.
・デバッグ.
