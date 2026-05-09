Anime Smoother OFX 0.9.10 - macOS

Anime Smoother OFX は、アニメ・線画・セル調画像のジャギーを軽減する OpenFX プラグインです。

確認済みホスト:
- Left Angle Autograph
- DaVinci Resolve

インストール方法:
1. このZIPを展開します。
2. AnimeSmoother フォルダを以下の場所へコピーします。

   /Library/OFX/Plugins/

   最終的に以下の形になればOKです。

   /Library/OFX/Plugins/AnimeSmoother/AnimeSmoother.ofx.bundle

3. Autograph または DaVinci Resolve を再起動します。
4. エフェクト一覧の以下の場所を確認してください。

   Filter / Anime / Anime Smoother

注意:
- /Library へのコピーには管理者パスワードが必要になる場合があります。
- 旧名の AnimeLineSmoother を入れている場合は、混乱を避けるため削除してください。
- 旧バージョンから更新して効果が出ない場合は、Autograph / Resolve を終了してから各ホストのOFXキャッシュを削除してください。
- ホストアプリを起動していた場合は、コピー後に必ず再起動してください。
- このプラグインはCPU処理のOpenFXエフェクトです。
- v0.9.7 ではRGB / RGBA / Alpha入力に対応しています。
- v0.9.8 では水平方向の帯分割によるマルチスレッド高速化プロトタイプを追加しています。
- v0.9.9 ではOFX入出力から内部処理への変換コピーを減らしたC++高速化プロトタイプを追加しています。
- v0.9.10 ではRelease最適化、リンク時最適化、ピクセル比較の高速化を追加しています。

同梱ファイル:
- AnimeSmoother.ofx.bundle
- README.txt
- LICENSE.txt
- THIRD_PARTY_NOTICES.txt
