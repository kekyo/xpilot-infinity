# XPilot Infinity

1991 年から続くマルチプレイヤー・オンライン・スペースアクション。

![XPilot Infinity](./images/xpilot-infinity.png)

Copyright © 1991-2005 by Bjørn Stabell, Ken Ronny Schouten, Bert Gijsbers,
Dick Balaska, Uoti Urpala, Juha Lindström, Kristian Söderblom and Erik
Andersson.

詳細については [COPYING](COPYING) を参照してください。すべての文書、ソースコード、
および COPYING ファイルを含めずに、このプロジェクトを配布することはできません。

> XPilot Infinity は XPilot NG から改名されました。
> XPilot Infinity の開発と配布は [xpilot-infinity リポジトリ](https://github.com/kekyo/xpilot-infinity/) で行われています。

[![プロジェクトの状態: 活動中 – プロジェクトは安定して利用可能な状態に達しており、活発に開発されています。](https://www.repostatus.org/badges/latest/active.svg)](https://www.repostatus.org/#active)

---

[(English language is here)](./README.md)

## XPilot Infinity とは？

[www.xpilot.org より](https://www.xpilot.org/about/):

XPilot はマルチプレイヤー対応の 2D スペースゲームです。Atari のアーケードゲーム
Asteroids や Gravitar、家庭用コンピューターゲームの Thrust（Commodore 64）や
Gravity Force（Commodore Amiga）といった名作から一部の要素を取り入れていますが、
XPilot には独自の新しい要素も数多くあります。

![XPilot](./images/game-client.png)

主な特徴:

- 完全なクライアント／サーバー方式のゲームで、すべてのプレイヤーに最適な速度を提供します。
- 世界各地でゲームをホストしているサーバーの最新情報を提供するメタサーバーがあります。
- 世界規模のレーティングサーバー網により、世界中のパイロットと腕を競い、世界ランキングを上げられます。
- 「リアルな物理演算」を採用しています。爆発の破片やエンジンから出る火花も、接触すれば機体に影響します。このため、エンジンの推力や爆発の衝撃波で相手を壁へ追い込み、倒すこともできます。
- 機体形状とマップを編集する専用エディターがあります。
- ゲームの目的やプレイ内容は、コマンドライン、専用のオプションファイル、またはマップファイルで指定する多数のオプションによって調整できます。ゲームモードの例:
  - クラシックなドッグファイト: 武器は銃のみで、操縦技術と戦術が勝敗を分けます
  - チーム戦: 仲間と協力し、他チームの宝物を奪い（Thrust のように、ひもにつながれたボールを運びます）、厳重に守られた標的を破壊します
  - 全面核戦争: 20 種類を超える武器・防御システムから慎重に装備を選び、生き残りながら敵を殲滅します
  - レース: 対戦相手より先に危険なコースを走破します
- 重力を調整できます。ワールド内に特殊な引力源や斥力源を配置したり、全体の重力をさまざまな方法で変更したりできます。
- 大砲や、個性的で執念深いロボット戦闘機が行く手を阻みます。
- エネルギーに注意し、手遅れになる前に燃料ステーションへ「ドッキング」して補給しましょう。
- 自分の本拠地を守ることも、他人の本拠地を襲って奪うこともできます。
- アフターバーナー、クローキング装置、センサー、転送装置、追加砲、機雷、爆弾、ロケット、スマートミサイル、魚雷（核弾頭を含む）、ECM、レーザー、追加タンク、オートパイロットなど、15 種類を超える防御・武器システムを機体に装備できます。

## インストールとドキュメント

[xpilot-infinity リポジトリ](https://github.com/kekyo/xpilot-infinity/releases/) でバイナリパッケージを入手できます。

対応パッケージの一覧は次のとおりです。

| ディストリビューション | リリース | アーキテクチャ |
| :--- | :--- | :--- |
| Debian | bookworm | amd64, i386, arm64, armhf |
| Debian | trixie | amd64, i386, arm64, armhf, riscv64 |
| Ubuntu | 22.04 | amd64, arm64 |
| Ubuntu | 24.04 | amd64, arm64 |
| Ubuntu | 26.04 | amd64, arm64 |
| Windows | - | x86（32 ビット）, x86_64（64 ビット） |

- 動作環境: SDL3、OpenGL 3.3、OpenAL（サウンド）

Debian および Ubuntu パッケージは、XDG 互換のアプリケーションメニューに
**XPilot Infinity** を追加します。ランチャーはパッケージ付属のアプリケーション
アイコンを使用し、追加の引数なしで SDL サーバーブラウザーを開きます。

### Windows へのインストール

Windows リリースでは、各アーキテクチャ向けに NSIS インストーラーと ZIP
アーカイブの両方を提供しています。通常のインストールでは、対応する
`xpilot-infinity-<version>-windows-<architecture>-setup.exe` を実行してください。
クライアント、サーバー、ゲームデータ、スタートメニューのショートカットが
インストールされます。スタートメニューから **XPilot Infinity** を選択すると、
SDL サーバーブラウザーが開きます。

ZIP アーカイブはポータブル版です。書き込み可能な任意のディレクトリに展開し、
`xpilot-infinity-sdl.exe` を直接実行してください。ポータブル版を使用する場合、
NSIS パッケージをインストールする必要はありません。

ソースコードからインストールする手順は [INSTALL](INSTALL) にあります。

## ゲームのクイックスタート

XPilot はサーバープログラムとクライアントプログラムで構成されています。
1 台のサーバーに複数のクライアントが接続するマルチプレイヤーゲームです。

最初にサーバーを起動します。

```bash
xpilot-infinity-server -noQuit
```

`-noQuit` は、すべてのクライアントが切断した後もサーバーを実行し続けるための
パラメーターです。同じサーバープロセスを引き続き利用できます。
サーバーを終了するには、Ctrl-C などを押すだけです。

サーバーの実行中に次のコマンドを入力すると、ローカルネットワークと
メタサーバーの両方から取得したサーバー一覧が表示されます。

```bash
xpilot-infinity-sdl
```

![サーバー一覧](./images/server-list.png)

> メタサーバーとは、全世界で公開されているXPilotのサーバー一覧を管理しているサーバーです。 xpilot.org によって運営されています。

サーバーへ接続してゲームを開始できます。

次のような URL を指定して、サーバーへ直接接続することもできます。

```bash
xpilot-infinity-sdl udp://localhost
```

`udp://` URL が示すとおり、この例では UDP プロトコルを使用します（UDPはデフォルトで、XPilot NGの最後のバージョンと互換性があります）。
TCP (`tcp://`) または WebSocket (`ws://`) も使用できます。

URL を指定せずに SDL クライアントを起動すると、メタサーバーへの問い合わせと、ローカルネットワーク上での 1 回の UDP 探索が行われます。サーバーブラウザーは両方の結果を統合して取得元を表示し、LAN サーバーを先頭に配置します。

サーバーは既定でUDPプロトコルで接続を受け付けるため、この手順で自動的にリストアップされるはずです。
`-localDiscovery no` を指定すると LAN への問い合わせを無効にできます。

ほかにも多くの機能がありますが、ここですべてを紹介することはできません。

XPilot Infinity のドキュメントは、まだ十分に整備されていません。詳しくは
[`doc/man`](doc/man) ディレクトリ内のマニュアルを参照してください。

---

## XPilot NG と XPilot Infinity の違い

XPilot Infinity は、現代の環境でも XPilot が快適に動作することを目標としています。
NG バージョン 4.7.3 からの主な変更点は次のとおりです。

- TCP と WebSocket のネットワークプロトコルを追加しました。これにより、ルーターやファイアウォールを通過しやすくなる場合があります。
- 現代的なグラフィックス環境へ更新しました。SDL3 へ移行し、OpenGL 3.3 コアプロファイルに対応しています。
- 内部構造を一部リファクタリングしました。
  これは、上記の変更を実現するためにも不可欠でした。
  ただし、バージョン 4.7.3 との UDP プロトコル互換性は維持しています。
- ユーザー名で TTF による多言語フォント表示に対応しました。
  現時点では、[Noto Sans Mono とそのファミリー](https://fonts.google.com/noto/specimen/Noto+Sans+Mono)をインストールしてください。
- サウンドエンジンを有効にしました。
- Windows バイナリを MinGW ツールチェーンでビルドできるように変更しました。
- 複数のアーキテクチャ向けに Debian パッケージ（*.deb）をビルドできるようになりました（Podman を使用）。

## ネットワークトランスポート

接続確認／ロビー接続とゲームプレイ接続は既定で UDP を使用し、それぞれを
個別に選択できます。両方で TCP を使用するには、次の例のようにサーバーと
クライアントの設定を一致させて起動します。

```console
xpilot-infinity-server -transport tcp [options]
xpilot-infinity-sdl -join tcp://server.example
```

両方で WebSocket を使用することもできます。

```console
xpilot-infinity-server -websocket [options]
xpilot-infinity-sdl -join ws://server.example
```

サーバーの短縮オプション `-tcp`、`-udp`、`-websocket`、および
`-transport udp|tcp|websocket` は、接続確認／ロビーとゲームプレイの両方に
同じトランスポートを指定します。コマンドラインのトランスポートオプションは
左から右へ適用されるため、後に指定した `contactTransport` または
`gameTransport` オプションを使って UDP/TCP の分離構成にすることもできます。
WebSocket は両方のトランスポートに指定する必要があります。

クライアントの直接接続先には、`HOST`、`ws://HOST[:PORT]`、
`tcp://HOST[:PORT]`、または `udp://HOST[:PORT]` を指定できます。
スキーム付きの接続先を指定すると、接続確認／ロビーとゲームプレイの両方で
そのトランスポートが選択されます。ホスト名だけを指定した場合は、
`contactTransport`、`gameTransport`、`port` オプションの既定値が使用されます。
接続先に明示したポートは `-port` より優先されます。UDP/TCP の分離構成は、
長形式オプションまたはメタサーバーの告知を通じて引き続き利用できます。

WebSocket は、`/xpilot` で `xpilot-infinity-v1` サブプロトコルを使用する
RFC 6455 バイナリメッセージとして実装され、各メッセージに 1 件の論理的な
XPilot レコードが格納されます。現在実装されているのは暗号化されていない
`ws://` のみです。このトランスポートはネイティブクライアントで使用できます。
ブラウザークライアントへの統合は、意図的に別の段階として扱っています。

たとえば `ws://server1 tcp://server2 udp://server3` のように複数の接続先を
指定すると、コマンドラインの順序で試行されます。これは接続先間の明示的な
フォールバックであり、クライアントが 1 つの接続先に対して複数のプロトコルを
自動的に試すものではありません。接続先ホストには DNS 名または IPv4 アドレスを
指定できます。IPv6 リテラルと、認証情報、パス、クエリー、フラグメント、
パーセントエンコーディングなどの一般的な URI 機能には対応していません。
LAN ブロードキャスト探索を利用できるのは UDP のみであるため、TCP および
WebSocket の接続確認には、明示的な接続先またはメタサーバーのエントリーが
必要です。明示的な接続先がない場合、SDL サーバーブラウザーはメタサーバーへの
問い合わせと並行して、LAN 上で 1 回の UDP 探索を行います。LAN とメタサーバーの
どちらか一方への問い合わせが失敗しても、もう一方の結果は利用できます。
エンドポイントとトランスポートの組み合わせが一致する結果は、1 件の
`LAN + Meta` エントリーとして表示されます。`-localDiscovery no` を指定すると
LAN への問い合わせを無効にできます。

明示的に指定したすべての接続先が、再試行後も有効な接続確認応答を返さなかった
場合、SDL クライアントは最後のエンドポイントと選択された 2 つのトランスポートを
含む最終的な概要を標準エラーへ出力します。通常のグラフィカル起動では、その概要を
モーダルエラーダイアログにも表示し、確認後にエラー終了します。`-text` または
`-list` を明示した場合は、ダイアログを開かず端末に報告します。`-list` を完了させる
ために使用されたサーバー応答や、後続の接続先から正常な応答が得られた場合は、
接続失敗として報告されません。

メタサーバーの告知には、両方のトランスポート設定が含まれます。SDL および X11
クライアントは、サーバーが選択されると告知された値を自動的に適用します。
トランスポートのメタデータを含まない従来の告知は、接続確認とゲームプレイの
どちらにも UDP を使用するサーバーとして扱われます。メタサーバーへの問い合わせと
報告の通信では、引き続き既存のプロトコルを使用します。

サーバーの `clientPortStart` から `clientPortEnd` までの範囲は、選択した
ゲームプレイプロトコルに適用されます。クライアントでは、ゲームプレイと従来の
UDP 接続確認ソケットにも適用されます。固定された TCP および WebSocket
セッションは、この範囲をローカルの送信元ポートとして使用します。

TCP ゲームプレイレコードは、ネットワークバイトオーダーの 2 バイトの
ペイロード長と、それに続く未変更の XPilot パケットペイロードで構成されます。
サーバーの録画は、録画時と同じ `gameTransport` の値を使用して再生する必要があります。

### systemd サービスとしてサーバーを実行する

Debian および Ubuntu パッケージは、任意で利用できる
`xpilot-infinity-server.service` をインストールします。パッケージを
インストールしただけでは、サービスは起動も有効化もされません。すぐに起動し、
以降のブート時にも起動するには、次のコマンドを実行します。

```bash
sudo systemctl enable --now xpilot-infinity-server.service
```

パッケージ付属のサービスは `ndh.xp2` マップを継続的に実行し、公開メタサーバーへ
自身を告知しません。既定のコマンドラインオプションは
`/etc/default/xpilot-infinity-server` に保存されています。別のマップを選択したり、
ほかのサーバーオプションを設定したりするには、そこにある
`XPILOT_SERVER_OPTIONS` を編集してからサービスを再起動します。

```bash
sudo systemctl restart xpilot-infinity-server.service
```

この設定ファイルでは、シェル構文ではなく systemd の `EnvironmentFile` 構文を
使用します。サーバーを公開する場合に限り、`+reportMeta` を `-reportMeta` に
置き換えてください。録画などの相対パスの出力ファイルは
`/var/lib/xpilot-infinity-server` 以下に書き込まれます。

現在の状態を確認し、サーバーログを継続表示するには次のコマンドを使用します。

```bash
systemctl status xpilot-infinity-server.service
journalctl -u xpilot-infinity-server.service -f
```

サーバーを停止し、以降のブート時に起動しないようにするには、次のコマンドを
実行します。

```bash
sudo systemctl disable --now xpilot-infinity-server.service
```

### Windows サービスとしてサーバーを実行する

インストーラーのコンポーネントページで、必要に応じて **Dedicated server
service (manual start)** を選択します。新規インストール時、このコンポーネントは
選択されていません。選択すると、`XPilotInfinityServer` は、スタートアップの種類を
**Manual**、実行アカウントを `LocalService` として登録されますが、インストーラーは
サービスを起動しません。既定のコンポーネント選択でインストールした場合、サービスは
登録されません。

管理者権限のコマンドプロンプトまたは PowerShell ウィンドウを開き、登録済みの
サーバーを手動で起動または停止します。

```bat
sc.exe start XPilotInfinityServer
sc.exe stop XPilotInfinityServer
```

以降の Windows 起動時にサービスを起動するには、スタートアップの種類を自動に
変更します。この設定コマンドだけでは、現在のセッションでサービスは起動しません。

```bat
sc.exe config XPilotInfinityServer start= auto
sc.exe start XPilotInfinityServer
```

手動起動へ戻すには、次のコマンドを使用します。

```bat
sc.exe stop XPilotInfinityServer
sc.exe config XPilotInfinityServer start= demand
```

サーバーの設定とログは `%ProgramData%\XPilot Infinity\server` 以下に保存されます。
既定の設定では、公開メタサーバーへ告知せずに `ndh.xp2` を継続的に実行します。
設定を変更するには、サービスを起動する前に `xpilot-infinity-server.conf` を
編集してください。アップグレードおよびアンインストールを行っても、このディレクトリは
保持されます。アンインストーラーはサービス自体を停止して削除します。

無人インストールでは、`/S` を指定すると通常のクライアント／サーバーファイルのみが
選択されます。オプションのサーバーサービスを登録するには `/SERVER_SERVICE=1` を追加します。
`/D` でインストールディレクトリを変更する場合、NSIS の仕様により最後の引数として
指定する必要があります。

```bat
xpilot-infinity-setup.exe /S /SERVER_SERVICE=1 /D=C:\Games\XPilotInfinity
```

### デスクトップからサーバーを操作する

アプリケーションメニューの XPilot Infinity Server Tray、または
`xpilot-infinity-tray` を起動します。トレイは一般ユーザー権限で動き、
登録済みのローカルサービスへ接続します。Linux では
`xpilot-infinity-server.service` を含む Debian パッケージを導入してください。
Windows ではインストーラーの任意選択のサーバーサービスを登録します。
トレイの起動だけでは、サービスの登録や開始は行いません。ZIP 版のトレイも
登録済みサービスへ接続できます。先に対応するサーバーと認可ヘルパーを、
管理者が保護するインストール先へ導入してください。

Start server と Stop server で開始・停止を操作します。必要に応じて Linux は
polkit、Windows は UAC の認証を求めます。マップの選択は共有設定を変更します。
稼働中の変更ではサービスを正常停止してから再起動するため、プレイヤーの
接続が切断されます。メニューに表示する設定マップは、稼働中のゲームが
実際に読み込んだマップを保証する表示ではありません。インストール済みの
マップは、トレイを起動した作業ディレクトリにかかわらず参照できます。

Edit configuration は、設定の編集用コピーを既定のテキストエディタで開きます。
UTF-8（BOM なし）で保存してから、トレイの Apply saved changes を選び、
反映を確認してください。エディタで保存するだけではサービスは変更されません。
停止中の反映は設定の保存のみ、稼働中の反映は保存と再起動を行います。
エディタは通常権限で起動します。自由編集の反映とマップ選択の認可は別です。

エディタで未保存の変更は反映しません。保存済みの未反映内容はトレイの再起動後も
残り、編集を再開できます。Discard editing copy でコピーを破棄し、やり直すことも
できます。別のセッションで共有設定が変更されると、古いコピーの反映は競合として
中止し、編集内容を保持します。残したい変更は、破棄する前に別途保存してください。
認証のキャンセルや保存失敗でもコピーを保持します。保存には成功したが再起動に
失敗した場合は区別して表示するので、詳細とサービスのログを確認してください。

共有設定は Linux では `/etc/default/xpilot-infinity-server`、Windows では
`%ProgramData%\XPilot Infinity\server\xpilot-infinity-server.conf` にあります。
Linux のログは `journalctl -u xpilot-infinity-server.service`、Windows のログは
`%ProgramData%\XPilot Infinity\server` で確認できます。詳細メニューにも
実際の設定とログの場所を表示します。操作対象はシステムサービスであり、
以下で説明する Podman のユーザーサービスは対象に含みません。

X11 と Wayland では StatusNotifierItem 対応のホストにアイコンを表示し、
X11 では XEmbed のトレイにも対応します。利用できるホストがない場合は
操作ウィンドウを表示します。トレイの終了、操作ウィンドウを閉じる操作、
ログアウトではサーバーを停止しません。停止するには Stop server を選んでください。

### Podman でサーバーを実行する

このリポジトリには、専用サーバー向けのマルチステージ
[`Dockerfile`](Dockerfile) が含まれています。ランタイムイメージには、サーバー、
標準マップ、設定データ、および必要な共有ライブラリだけが含まれています。
UID/GID `10001:10001` で実行され、既定のコマンドは公開メタサーバーへ報告せずに
TCP ポート 15345 で `ndh.xp2` を起動します。

このリポジトリから公開されるイメージの名前は
`docker.io/kekyo/xpilot-infinity-server` です。代わりにローカルイメージを
ビルドするには、次のコマンドを実行します。

```bash
image=localhost/xpilot-infinity-server:local
./build_container_image.sh --tag "$image"
```

このビルドコマンドは Podman を使用し、イメージをプッシュすることはありません。
リリースおよびマルチアーキテクチャビルドの手順は
[`doc/PACKAGING.md`](doc/PACKAGING.md#dedicated-server-container-image) にあります。

録画など、相対パスで出力されるファイル用の名前付きボリュームを作成し、
読み取り専用のルートファイルシステムかつ Linux ケーパビリティなしで、既定の
TCP サーバーを起動します。

```bash
image=docker.io/kekyo/xpilot-infinity-server:latest
podman volume create xpilot-infinity-server-data
podman run --detach \
  --name xpilot-infinity-server \
  --read-only \
  --cap-drop=all \
  --security-opt=no-new-privileges \
  --publish 15345:15345/tcp \
  --volume \
    xpilot-infinity-server-data:/var/lib/xpilot-infinity-server \
  "$image"
```

ローカルクライアントからのみ接続させる場合は、公開ポートを `127.0.0.1` に
バインドしてください。リモートクライアントは、次のように既定のコンテナへ接続します。

```bash
xpilot-infinity-sdl tcp://server.example:15345
```

イメージ名の後に指定した引数は、イメージの既定のサーバー引数を置き換えます。
オプションを追加する場合は、`-noQuit`、`+reportMeta`、マップ、および `-tcp` も
再度指定してください。たとえば、独自の既定値ファイルとマップを読み取り専用で
マウントできます。

```bash
podman run --detach \
  --name xpilot-infinity-server \
  --read-only --cap-drop=all \
  --security-opt=no-new-privileges \
  --publish 15345:15345/tcp \
  --volume "$PWD/defaults.txt:/etc/xpilot/defaults.txt:ro" \
  --volume "$PWD/my-map.xp2:/maps/my-map.xp2:ro" \
  --volume \
    xpilot-infinity-server-data:/var/lib/xpilot-infinity-server \
  "$image" \
  -noQuit +reportMeta -tcp \
  -defaultsFileName /etc/xpilot/defaults.txt \
  -map /maps/my-map.xp2
```

SELinux ホストでは、ホストのバインドマウントに適切な `z` または `Z` の
再ラベル付けオプションを追加してください。同梱のマップは、マウントせずに
ベース名で選択できます。

管理者パスワードはイメージとソースツリーの外部で管理してください。
[`lib/password.txt`](lib/password.txt) に記載された形式でファイルを作成し、
Podman のシークレットとしてインポートして、イメージの非 root ユーザー向けに
マウントします。

```bash
podman secret create xpilot-server-password ./password.txt
podman run --detach \
  --name xpilot-infinity-server \
  --read-only --cap-drop=all \
  --security-opt=no-new-privileges \
  --publish 15345:15345/tcp \
  --volume \
    xpilot-infinity-server-data:/var/lib/xpilot-infinity-server \
  --secret \
    source=xpilot-server-password,target=xpilot-password,uid=10001,gid=10001,mode=0400 \
  "$image" \
  -noQuit +reportMeta -map ndh.xp2 -tcp \
  -passwordFileName /run/secrets/xpilot-password
```

従来の UDP トランスポートでは、接続確認ポートと、固定された 1 対 1 の
ゲームプレイ用ポート範囲の両方が必要です。イメージは意図的に既定の TCP
ポートだけを公開しているため、UDP では `--publish-all` を使用しないでください。

```bash
podman run --detach \
  --name xpilot-infinity-server-udp \
  --read-only --cap-drop=all \
  --security-opt=no-new-privileges \
  --publish 15345:15345/udp \
  --publish 15346-15445:15346-15445/udp \
  "$image" \
  -noQuit +reportMeta -map ndh.xp2 -udp \
  -clientPortStart 15346 -clientPortEnd 15445
```

これらの公開ポートを通じて UDP で直接接続できます。UDP の LAN ブロードキャスト
探索は、rootless コンテナネットワークを越えられない場合があります。探索が必要な
場合は `--network=host` を明示的な代替手段として使用できますが、ネットワーク
名前空間による分離がなくなるため、ポート公開オプションと併用してはなりません。
rootless のポートフォワーディングによって接続元クライアントのアドレスが隠れる
場合もあります。IP アドレスに基づくプレイヤー数制限を変更する際は考慮してください。

ログを継続表示し、サーバーを正常に停止するには次のコマンドを使用します。

```bash
podman logs --follow xpilot-infinity-server
podman stop xpilot-infinity-server
```

`podman stop` はイメージに設定された停止シグナル `SIGTERM` を送信するため、
サーバーは終了前に永続化する出力を書き出せます。

rootless systemd で任意に管理する場合は、付属の Quadlet ファイルをユーザーの
ジェネレーターディレクトリへコピーします。

```bash
quadlet_dir="${XDG_CONFIG_HOME:-$HOME/.config}/containers/systemd"
mkdir -p "$quadlet_dir"
cp containers/xpilot-infinity-server.container "$quadlet_dir/"
cp containers/xpilot-infinity-server-data.volume "$quadlet_dir/"
systemctl --user daemon-reload
systemctl --user start xpilot-infinity-server.service
```

Quadlet は `latest` イメージ、同じセキュリティ強化済みの TCP 設定、および永続的な
名前付きボリュームを使用します。その `[Install]` セクションにより、以降の
ユーザー systemd セッションで起動します。生成された Quadlet サービスは
`systemctl enable` で有効化しません。対話的ログインなしでブート時に起動するには、
管理者がそのアカウントの linger を追加で有効にできます。状態は
`systemctl --user status xpilot-infinity-server.service` と
`journalctl --user -u xpilot-infinity-server.service` で確認できます。

サービスの起動前にシークレットが存在している必要があるため、Quadlet は既定で
シークレットを参照しません。シークレットを作成した後、`Secret=` 行を
`[Container]` セクションに追加し、前述の手動 Podman の例と同様に
`-passwordFileName` を `Exec=` へ追加してください。サービスを再起動する前に、
検証済みの新しいイメージを明示的にプルしてください。レジストリからの自動更新は有効に
なっていません。

---

## その他の情報源

- [XPilot NG](http://xpilot.sourceforge.net/)
- XPilot FAQ: `telnet meta.xpilot.org 4402`（`doc` ディレクトリにも収録）
- [XPilot ウェブサイト](http://www.xpilot.org/)
- [Windows 版 XPilot](http://www.buckosoft.com/xpilot/)
- [XPilot 初心者向けマニュアル](http://bau2.uibk.ac.at/erwin/NM/www)
- [XPilot FTP アーカイブ](ftp://ftp.xpilot.org/pub/)
- XPilot ニュースグループ: `rec.games.computer.xpilot`

## 提供されたソフトウェア

追加プログラムについては、`contrib` ディレクトリを確認してください。

同梱の xp2 マップエディターもお試しください。新しい XPilot ワールドを設計できる
グラフィカルエディターです。

## ライセンス

Under GPLv2
