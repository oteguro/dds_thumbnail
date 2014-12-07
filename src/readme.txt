UTF8
--------
※開発中の為、.ddsファイルを.hogeを拡張子に変えたファイルのみサムネイル表示します。
※まだ十分にテストされていません。

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

--------
残作業

・BC4, BC5, BC6, BC7の実装、確認.
・FP32, FP16テクスチャの実装、確認.
・キューブマップ、ボリュームテクスチャの動作確認.
・その他マイナーフォーマットの確認.
・パフォーマンスに関する考慮.
・デバッグ.
