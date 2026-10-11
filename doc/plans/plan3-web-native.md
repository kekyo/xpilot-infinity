# XPilot Infinity ブラウザクライアントとネイティブサーバー対応計画

作成日: 2026-10-11

## 1. 目的と合意事項

XPilot Infinity の既存 C クライアントを Emscripten で WebAssembly にビルドし、SDL3・WebGL 2 を使ってブラウザでプレイできるようにする。ブラウザは XPilot のコンテンツを提供したサーバーへ接続する。ゲームサーバー本体はネイティブ版を使用する。

利用者との検討で合意した前提は次のとおり。

- ブラウザ版にメタサーバー接続、LAN 探索、UDP ping、サーバー選択画面は不要。配信元のゲームへの参加に限定する。
- ネイティブサーバーの既存 WebSocket プロトコルを利用する。サーバーの Node.js＋WASM 化は今回の前提にしない。
- フォントは XPilot Infinity の同梱フォント、またはその Web 向け変換物を使用する。
- フォールバック文字は要求時に JavaScript で画面外のビットマップへ描画し、そのビットマップをゲーム描画に利用する方式を採用できる。
- フォールバックは文字列単位で行い、同梱フォントを優先したブラウザのフォント選択を利用する。大量の多言語フォントの同梱を必須にしない。
- 各段階で実行可能な成果物を用意し、テストの RED→GREEN を確認して適切な単位でコミットする。

ユーザーはこの計画の保存と作業開始を承認済みである。通常の実装・検証を進める際に再承認を求めない。必要なビルド・テスト用パッケージが未導入なら、AGENTS.md に従って導入を促す。

## 2. 対象範囲と開始時の状態

2026-10-11 の追加指示により、ブラウザ対応を XPilot Infinity 本体へ正式に統合する。計画の正本は本体の `doc/plans/plan3-web-native.md`、ブラウザアプリは本体の `xpilot-browser/` とする。以下の開始時記録は経緯として保持する。

- 親リポジトリ: `/home/kouji/Projects/xpilot-ng-browser`、ブランチ `develop`。
- 本体リポジトリ: `xpilot-infinity/`、ブランチ `feature/browser`。調査時 HEAD は `11e51fa`。
- 開始時の親 `git status --short` は ` M xpilot-infinity`。本体内の作業ツリーは clean。この既存の参照差分は無関係な変更として親のコミットへ含めない。
- 変更対象は本体の `xpilot-browser/`、配信・ビルド・テスト設定、およびクライアント・サーバー・共通層である。親の `xpilot-client/` は段階 1a で移す。
- `xpilot-infinity/vendor/` の外部コードと、そのサブモジュール参照は変更しない。依存ライブラリは提供されたビルド機能で別ディレクトリへビルドする。
- 正式実装・計画・検証を本体リポジトリでコミットする。親には移行案内と移行に必要なワークスペース整理を残す。既存の本体コミットを維持したうえで、最終的なサブモジュール参照を更新する。バージョン更新が必要なら screw-up を使う。

## 3. コード調査結果

以下のパスは、特記しない限り `xpilot-infinity/` を基準とする。

| 項目 | 確認した現状 | 設計への反映 |
| --- | --- | --- |
| `src/client/sdl/sdlinit.c:244` | SDL3 が OpenGL 3.3 Core を要求する。現時点で OpenGL ES への移行が完了しているわけではない。 | ブラウザ用の GLES 3.0／WebGL 2 コンテキストを追加する。 |
| `src/client/sdl/renderer_gl_core.c:139` | GLSL 330 Core、`glPolygonMode`、デスクトップ GL 固有の状態と能力判定を使用する。 | GLES 対応シェーダー、関数と状態の差をバックエンドに局所化する。 |
| `src/client/sdl/renderer_backend.h:28` | 描画コマンド、テクスチャ、メッシュ、コンテキスト復旧がインターフェイス化されている。 | ゲーム描画、三角形化、アトラスを再利用する。バックエンド全体の無条件コピーは避ける。 |
| `src/common/websocket_http.h:18` | リソースは `/xpilot`、サブプロトコルは `xpilot-infinity-v1`。 | ブラウザも同じプロトコルを指定する。 |
| `src/common/websocket_transport.h` | バイナリ WebSocket メッセージ 1 件につき論理レコード 1 件。 | ブラウザ API にフレーミングとマスキングを任せる。TCP 用の長さヘッダーを加えない。 |
| `src/common/record_transport.h:90` | 送受信、flush、終了、任意の native handle が操作テーブル化されている。 | ブラウザ WebSocket バックエンドを実装し、上位のレコード・セッション処理を利用する。 |
| `src/client/session_connection.c:72`、`:253` | 参加したセッションをゲームへ引き継ぐ。接続待機は native handle と select に依存する。 | 同じ WebSocket を参加からプレイまで使い、待機を step 駆動へ移す。 |
| `src/client/sdl/gameloop.c:31`、`:88`、`:96` | step は存在するが native fd を必須とし、通常ループは select で待つ。 | ネイティブの待機と共通の進行処理を分離する。 |
| `src/client/netclient.c` | 検証、初期化、再接続、終了にも同期的な待機がある。 | main loop の変更だけで終わらせず、接続ライフサイクル全体を非同期化する。 |
| `src/client/connect_target.c:128` | `ws://` は扱うが `wss://` やパス付き URL は扱わない。 | ブラウザ境界には配信元から生成した完全な WebSocket URL を渡す。 |
| `src/client/native_text.c:686` | Windows API または Fontconfig でフォントを供給する。 | ブラウザ用の文字サービスを追加し、ブラウザビルドから OS フォント検索を切り離す。 |
| `src/client/sdl/text_atlas.c`、`text.c:282` | ASCII は字形アトラス、非 ASCII は Unicode 文字列経路に分かれる。 | 既存 ASCII 表示を活用し、Unicode 側へ Canvas プロバイダーを接続する。 |
| `src/client/sdl/unicode_text_renderer.c:155`、`:312` | レイアウトを測定し、必要時に RGBA ビットマップを生成してテクスチャをキャッシュする。現在の上限は 128 件。 | Canvas のベーク結果を渡し、既存のキャッシュと描画を利用する。 |
| 同 `:328` | 論理レイアウトとビットマップ寸法が一致する前提。 | 文字送り幅、インク範囲、ベースライン、描画オフセットを分離し、斜体の欠けを防ぐ。 |
| `lib/fonts/Makefile.am` | `VeraMoBd.ttf`、`FreeSansBoldOblique.ttf`、`ConsoleFont.bmp` を配布する。 | これらを基本フォントとし、フォント一式の追加配布を前提としない。 |
| `src/client/oalaudio.c` | OpenAL と ALUT を使用する。 | Emscripten OpenAL とファイルデコードを分けて検証する。 |
| 親 `xpilot-client/vite.config.ts` | emsdk-env と standalone WASM ローダーの雛形。アプリ実装は未着手。 | JS ランタイムを含む `.mjs`＋`.wasm`＋アセットの出力・ロードを検証する。 |
| `src/common/websocket_http.c:503` | HTTP は Upgrade 専用で、別リソースの GET は 404。 | 通常 HTTP と Upgrade を振り分ける配信入口が必要。 |
| `src/server/contact.c:786` | 管理操作の所有者判定に loopback IP と所有者名を使用する。 | 公開プロキシからの接続をローカル管理者として扱わない。 |
| `src/server/contact.c`、`netserver.c` | 接続元 IP を参加待ち上限、人数制限、拒否、再接続へ使用する。 | プロキシ採用時には信頼境界を明示して実 IP を受け渡す。 |
| `src/server/sched.c:311` | サーバーは select を使う同期ループで、シグナル等にも依存する。 | Node.js＋WASM 化は単なるビルド変更ではなく、別の移植課題として扱う。 |

## 4. 利用者から見た動作

利用者はサーバーの URL を開き、読み込み完了後に名前を入力して参加する。ブラウザ側にサーバー一覧や任意ホストの探索機能は設けない。参加操作は音声や相対マウス等の利用者操作を必要とする機能の開始点にもする。

接続先は原則として同じ公開ホスト・ポートの `/xpilot` とする。HTTP なら `ws:`、HTTPS なら `wss:` に対応させる。ページとゲームが同じ公開オリジンとなる構成を配布時の標準とする。開発時は Vite の配信入口からローカルのネイティブサーバーへ接続する。

読み込み中、接続中、参加拒否、プレイ中、切断と再接続の状態を表示する。接続失敗やサーバー不在でも画面が応答し、再試行できる。ゲーム画面と HUD は既存 C 描画を使い、ブラウザ UI は参加と状態表示など必要な範囲に絞る。

初期対象はキーボード・マウス操作が可能なデスクトップブラウザ。タッチ専用 UI、WebGL 1、サーバー一覧、新たな RTL 対応は今回の追加対象にしない。

## 5. クライアントの設計

### 5.1 ビルドとランタイム

TypeScript、Vite、Vitest、npm、emsdk-env Vite プラグインを使う。prettier-max と resolved-killer は公式資料および API 定義を確認して適用する。C コードは C99 と既存の Makefile 構成を基本にし、依存ライブラリの提供するビルド手段はそのまま利用する。

SDL3、必要な SDL_image／SDL_ttf とその依存、libtess2 等は WASM 用にビルドする。ホスト用のライブラリ、Fontconfig、X11、トレイ、サーバー実行部分をブラウザへリンクしない。SDK と依存バージョンを固定し、暗黙の latest 更新を避ける。

Emscripten の JavaScript ランタイムを利用する。現在の汎用 `WebAssembly.instantiate` ローダーだけで SDL・WebGL・ファイルシステムを動かそうとしない。emsdk-env の出力設定を使って通常のモジュール化されたランタイムを生成し、Vite 側から初期化する。初版は pthread を使わず、COOP／COEP を必須条件にしない。

### 5.2 描画

WebGL 2 必須とし、GLES 3.0 対応のシェーダーと GL 操作を用いる。既存 Renderer の意味上の描画命令、RGBA8 テクスチャ、三角形メッシュを共有する。Core 専用操作はバックエンド境界に局所化し、不要になった処理は削除する。既存のネイティブ描画は同じ意味の描画を維持する。

高 DPI とキャンバスサイズ変更を、論理座標とピクセル座標を分けて処理する。コンテキスト喪失時は描画を止め、復旧時には既存の復旧契約を通してメッシュ・テクスチャを再構築する。

### 5.3 通信とイベントループ

ブラウザ WebSocket の open／message／error／close をレコードトランスポートへ接続する。ネイティブの wslay と POSIX ソケット経路はブラウザでは使わない。バイナリメッセージ境界、最大サイズ、必須レコードと一時レコードの保持方針を維持する。

送受信キューに上限を設け、送信済みとブラウザへ投入済みを混同しない。WebSocket の bufferedAmount を考慮し、過負荷時に無制限に蓄積させない。必須レコードを恣意的に捨てず、維持できない状態は明示的に切断する。

接続、セッション参加、検証、初期データ、プレイ、再接続、終了を短時間で戻る step として進める。SDL3 コールバックまたは Emscripten メインループから駆動する。描画頻度とネットワーク処理の進行を同一視しない。タブ非表示による停止から復帰したときに入力の押下状態や古いキューを引きずらない。

WebSocket 参加時に開いた接続をプレイへ引き継ぐ。再接続時は同じ配信元へ接続し、既存のセッショントークン手続きを使う。メタサーバー処理を省いても、ゲーム参加のプロトコル手続きを省略しない。

### 5.4 フォントと文字

同梱フォントを使う基本描画と、JS Canvas による要求時の Unicode ベークを組み合わせる。Canvas 側も同梱フォントを優先し、続く generic family を通してブラウザにフォールバックを任せる。フォント変換が必要ならビルドで生成し、外部フォントサービスを実行時の必須依存にしない。

ASCII アトラスは既存機構を活用する。Unicode は名前・チャットの表示行などの文字列全体を測定・描画する。不足したコードポイントだけを分離して継ぎ合わせ、結合文字や字間調整を壊す方法は取らない。

JS と C の境界は文字列・書体・サイズ・言語・方向の要求と、論理幅、行間、基線、ピクセル範囲、描画オフセット、RGBA8 の結果とする。既存 API の不足部分を最小限拡張し、測定と描画の条件を一致させる。Canvas の送り幅をインク範囲と同一視せず、同梱の斜体を含めて欠けを検証する。

同梱 Web フォントの読み込み完了を待ってからベークする。キャッシュには文字列・書体・サイズ・描画倍率・フォント世代を反映し、メモリーを制限する。倍率変更やフォント更新では必要な再生成を行う。ブラウザフォールバックの字形は端末に依存するため、全 OS で同じピクセルになることを要件にしない。

Canvas はフォールバックに使った字体や全 Unicode の字形有無を直接列挙する API ではない。現在の `has_glyph` 等の契約を幅比較で偽装せず、利用箇所を確認し、同梱フォントの検査とブラウザによる実描画を区別する。

### 5.5 アセット・設定・音声・入力

読み取り専用アセットは配信後に仮想ファイルシステムへ配置する。既存のデータパスをブラウザ用に設定する。設定は IndexedDB 等へ保存し、起動時の読み込みと変更時の保存を明示する。ページ終了時だけの保存には依存しない。

OpenAL は Emscripten の Web Audio 実装を候補とし、ALUT のファイル読み込み部分を別に検証する。参加操作後の音声開始、ミュート、ループ音の停止、再参加時の解放を確認する。

フォーカス喪失、キー解放、相対マウス、全画面、キャンバスのサイズ変更を扱う。ブラウザが提供する利用者操作の制約に従い、失敗時にも通常操作へ戻れる構成にする。

## 6. サーバーと配信

ゲーム本体は既存のネイティブ版を使用する。配信元への接続という利用者仕様と、HTTP とゲームが同じプロセスかどうかは分けて考える。

今回の実装ではネイティブサーバーへの静的 HTTP 配信を基本とし、WebSocket 待受の HTTP 前段で通常 GET／HEAD と `/xpilot` の Upgrade を振り分ける。静的ルートは明示した配信ディレクトリ配下に限定する。パストラバーサル、存在しないファイル、サイズ上限、切断途中を扱い、ファイル送信がゲーム進行を止めないようにする。HTML、JS、WASM、画像、フォント、音声を正しい MIME で配信する。

公開 HTTPS／WSS は既存の TLS フロントを使える構成とする。プロキシを使う場合には接続元 IP と管理認可の問題を先に解決する。信頼するプロキシを明示して接続元情報を検証し、クライアントが任意の転送ヘッダーを名乗っても信用しない。ブラウザ向けの公開入口から、loopback 由来というだけで管理者権限を得られない構成にする。通常の人数制限と再接続判定も実接続元の意味を保持する。

ブラウザ公開入口の Origin の扱いを明示し、配信元への接続という仕様に整合させる。静的配信を無効にした従来のネイティブ運用も不必要に変更しない。

検討した代替案と判断は以下のとおり。

| 代替案 | 利点 | 今回の判断 |
| --- | --- | --- |
| HTTP フロントが静的配信と WS 転送を担当 | TLS・配信をゲームから分離できる | 開発や公開時に利用できる。ただし実 IP・管理認可への対応を省略しない。 |
| Node.js＋WASM ゲームサーバー | Node に I/O と配信を集約し、WASM を配布できる | 今回のクライアント移植から外す。同期スケジューラー、シグナル、ソケット、ファイル境界の移植が別途必要。 |
| Node.js＋WASM で WS のみ提供 | 初期の実装範囲を絞れる | 技術的な必須制約ではない。Node の net/dgram 等を橋渡しすれば TCP/UDP も設計可能。 |
| Asyncify により既存待機を維持 | 一部の同期処理を早期に動かせる場合がある | 既存 step 関数を活用した非同期状態遷移を優先する。 |

## 7. 反復実装の段階と完了条件

### 段階 1: WASM 描画ドライバー

SDK・依存ビルド・emsdk-env のモジュール出力を用意する。実際の Renderer と WebGL 2 を使うブラウザドライバーを作り、図形・画像・同梱フォントを表示し、入力とサイズ変更を観測できるようにする。Unicode ベークと斜体の描画境界をこのドライバーで検証する。

完了条件:

- npm から再現可能にビルドでき、ブラウザで WASM と Emscripten ランタイムを初期化できる。
- 実 Renderer の図形、RGBA 画像、同梱フォント、ブラウザフォールバック文字列が表示される。
- 基本入力とサイズ変更が動き、文字の測定と描画が一致する。
- 対応するテストで RED→GREEN を確認し、変更したネイティブ境界のテストも通る。
- 観測可能な成果物と検証記録を残し、実装をコミットする。

### 段階 1a: 本体への正式統合

親の `xpilot-client/` を本体の `xpilot-browser/` へ移す。npm パッケージ名・ランタイム名・ソース参照・フォント参照を本体内の配置に合わせ、独立した lockfile を用意する。既存の Renderer プレビュー、Canvas フォント、Makefile、Vitest／Playwright MCP のテストを維持する。生成済み CMake キャッシュは移さず、新しい場所で依存を構築する。

ブラウザはアプリとして配布し、未実装のライブラリエクスポートを持たせない。親の Node.js＋WASM サーバー雛形を本体の必須ビルドへ持ち込まない。本体の利用者向け README からブラウザの起動方法へ案内する。従来のネイティブ Autotools／Makefile ビルドは引き続き独立して実行可能とする。

完了条件:

- 本体と本体内の vendor サブモジュールだけで、`npm --prefix xpilot-browser ci` と build／test を実行できる。
- 本体内のソース・フォントを使用して WASM をビルドし、移行前と同じプレビューの機能テストが通る。
- 計画書、実装、ブラウザ用 lockfile、検証記録が本体に揃い、親の旧ワークスペースへの依存がない。
- 移行をコミットしてから段階 2 以降の全項目へ進む。

### 段階 2: 配信元への非同期参加

ブラウザ WebSocket のレコードアダプターと参加状態機械を実装する。段階 1 のドライバーに接続状態を表示し、ネイティブサーバーから初期データを取得する。

完了条件:

- 配信元から決めた `/xpilot` に接続し、既存サブプロトコルで参加できる。
- サーバー不在、参加拒否、切断、キュー上限を処理して画面が応答する。
- メタサーバー、LAN 探索、UDP に依存しない。
- レコード境界と必須メッセージ保持を機能テストで確認してコミットする。

### 段階 3: ゲームプレイ

クライアントの初期化・ゲームループ・描画・入力を状態機械へ統合する。再接続と終了も非同期にする。

完了条件:

- 複数ブラウザが同じネイティブゲームへ参加できる。
- 操縦、射撃、HUD、チャット、切断、再参加が動作する。
- ネイティブクライアントとの同時参加を確認する。
- 操作と画面推移を動画付きのブラウザテストで検証してコミットする。

### 段階 4: ブラウザでの継続利用

音声、設定永続化、フォーカスとタブ復帰、相対マウスと全画面、高 DPI、WebGL コンテキスト復旧を統合する。

完了条件:

- 再読み込みで設定が復元され、ユーザー操作後に音声が再生される。
- フォーカス喪失や復帰で入力が押しっぱなしにならない。
- サイズ変更と描画復旧後も図形・文字・画像が正しく表示される。
- 再参加を繰り返しても音声やテクスチャ等を蓄積させない。
- 対応する機能テストが通り、実装をコミットする。

### 段階 5: ネイティブ配信と全体検証

ネイティブ HTTP 配信とブラウザ成果物を統合し、TLS フロントを含む公開構成を検証する。利用者向けに起動・参加・設定方法を文書化する。

完了条件:

- 配信元 URL を開くだけで、そのサーバーへ参加できる。
- HTML・JS・WASM・アセットの配信と WS Upgrade が同じ公開入口で動作する。
- HTTPS／WSS、同時接続、接続元 IP、管理操作の制限が正しく機能する。
- npm の全体テスト、変更対象を含む既存ネイティブ全体テスト、ブラウザ E2E を実行して結果を記録する。
- 計画の全完了条件を照合し、残件があれば明記する。成功していない検証を成功と記録しない。
- 変更を適切な粒度でコミットし、作業ツリーの状態を報告する。

## 8. 検証方針

バグや機能不足を表す動作テストを先に追加し、RED を確認してから実装する。ソースに特定の文字列があることを検査する方法は、機能テストの代用にしない。機能削除の場合は削除を検証するテストの RED→GREEN と一旦のコミットを行い、その後一時テストを除去する。

C の単体テストと実 Renderer のドライバーを使い、TS の境界は Vitest で検証する。ブラウザ試行・E2E は Playwright MCP を利用する。時間軸の挙動は動画で観測し、その結果を検証する。画像マスターを作成・変更する場合は必ず目視する。フォールバック字形は OS 依存なので、全環境で同一画像を要求せず、同梱フォントの検証と可読性・配置・欠けの検証を区別する。

テストは固定 sleep に成功を依存させず、初期化、接続、描画、入力結果などの観測可能な状態を待つ。必要に応じて時間制限を伸ばし、エミュレーションや低速環境でも本質的な失敗を検出する。

計画外の問題が判明した場合は根本原因と計画への影響を先に分析し、必要な計画修正を記録してから進める。場当たり的な変更を重ねない。

## 9. 参照した公式資料

- [Emscripten OpenGL support](https://emscripten.org/docs/porting/multimedia_and_graphics/OpenGL-support.html)
- [SDL3 Emscripten](https://wiki.libsdl.org/SDL3/README-emscripten)
- [Emscripten networking](https://emscripten.org/docs/porting/networking.html)
- [Emscripten WebSocket API ヘッダー](https://github.com/emscripten-core/emscripten/blob/main/system/include/emscripten/websocket.h)
- [Emscripten runtime environment](https://emscripten.org/docs/porting/emscripten-runtime-environment.html)
- [Emscripten modularized output](https://emscripten.org/docs/compiling/Modularized-Output.html)
- [Emscripten file system](https://emscripten.org/docs/api_reference/Filesystem-API.html)
- [Emscripten audio](https://emscripten.org/docs/porting/Audio.html)
- [Emscripten deployment](https://emscripten.org/docs/compiling/Deploying-Pages.html)
- [emsdk-env](https://github.com/kekyo/emsdk-env)
- [Canvas 仕様](https://html.spec.whatwg.org/multipage/canvas.html)
- [CSS Font Loading API](https://developer.mozilla.org/en-US/docs/Web/API/CSS_Font_Loading_API)
- [Canvas TextMetrics](https://developer.mozilla.org/en-US/docs/Web/API/TextMetrics)
- [WebSocket API とバックプレッシャー](https://developer.mozilla.org/en-US/docs/Web/API/WebSocket)
- [WebSocket のセキュア接続](https://developer.mozilla.org/en-US/docs/Web/API/WebSockets_API/Writing_WebSocket_client_applications#security_considerations)
- [Node.js net](https://nodejs.org/api/net.html)、[Node.js dgram](https://nodejs.org/api/dgram.html)

公式資料の最新ページと固定 SDK の API が異なる場合は、固定版のヘッダー・コメント・実装で確認する。外部ライブラリの API 使用前には公式文書と API コメントの両方を確認し、非推奨・内部 API を採用しない。

## 10. 作業記録

- 2026-10-11: 検討内容とユーザーの追加条件を保存。段階 1 に着手。開始時点ではビルド・テスト未実施。
- 2026-10-11: 段階 1 の実ブラウザ試験で、同梱 FreeSansBoldOblique.ttf の空 GDEF テーブルが Chrome の OpenType Sanitizer に拒否されることを確認。想定済みの Web 向け変換として、空テーブルを除去し SFNT のディレクトリとチェックサムを再計算した配信用コピーを生成する。字形と非空テーブルは保持し、元アセットと外部コードは変更しない。根拠: [OpenType フォント構造](https://learn.microsoft.com/en-us/typography/opentype/spec/otff)、[head チェックサム](https://learn.microsoft.com/en-us/typography/opentype/spec/head)、[OTS のテーブル検証](https://github.com/khaledhosny/ots/blob/main/src/ots.cc)。

### 段階 1 の成果物と検証（2026-10-11）

- Emscripten 5.0.1、SDL3、既存 `renderer.c` と共有 GL バックエンドを npm／Vite でビルドし、モジュール化した Emscripten ランタイムをブラウザから起動できるようにした。SDK 別ディレクトリと `flock` により SDL 依存の取り違え・同時構築の競合を防ぐ。
- `Renderer_gl_es_create` を追加。GLSL ES 3.00、GLES 非対応関数・状態の分岐、コンテキスト判定を共有バックエンドへ局所化した。本体コミットは `766d150`（`feat: add OpenGL ES 3 renderer backend`）。
- ブラウザドライバーは図形、RGBA チェッカー画像、同梱の等幅フォント、斜体と日本語・結合文字を表示する。キー操作、マウス位置、リサイズ、描画倍率 1／2 を確認した。
- Canvas で行全体をベークし、文字送り幅・インク範囲・基線・倍率を分離。128 件／8 MiB のキャッシュ上限を設けた。ドライバーの文字はこのサービスを使う。ゲーム本体の ASCII アトラス／Unicode レンダラーへの統合は段階 3 で行う。
- 文字のアルファ合成、物理ビットマップ寸法と論理描画寸法の混同、ページ保持・復帰イベント後の描画停止を、それぞれ RED→GREEN で修正した。ページ保持時は描画を停止してリソースを保ち、復帰時に再開する。実際の履歴キャッシュやコンテキスト喪失を含む総合ライフサイクル検証は段階 4 に残す。
- Playwright MCP の通常ツールを MCP SDK から呼び出す E2E を追加。実ピクセル、キー入力による移動、リサイズの完了、フォントのクリップとキャッシュ、保持・復帰イベントを確認し、入力動画を記録する。ブラウザの非同期リサイズは観測可能な寸法の一致を待つ。並列ビルド試験も固定 sleep を使わず FIFO とイベントで同期する。

検証結果:

| 検証 | 結果 |
| --- | --- |
| 固定 SDK の新規 SDL クロスビルドとクライアント Vite ビルド | 成功。`xpilot-client/dist/` に HTML、JS、WASM、フォントを出力。 |
| `npm exec --workspace=xpilot-client -- vitest run --config vitest.config.ts` | 5 ファイル、14 件成功。ブラウザ倍率 1／2 の E2E を含む。 |
| `npm exec --workspace=xpilot-client -- tsc --noEmit` | 成功。アプリ・ビルド設定・テストを対象。 |
| `make -C xpilot-client/wasm check NATIVE_PKG_CONFIG_PATH=/tmp/xpilot-multiple/vendor-prefix/lib/pkgconfig` | 成功。実 GLES3 の文字合成・論理寸法・入力・リサイズ。 |
| `make -C /tmp/xpilot-multiple/native-build check` | tray 10 件＋本体 83 件、計 93 件成功。失敗・スキップなし。 |
| 親の `npm test` | クライアントに加え全ワークスペースを実行。既存 `xpilot-server` 雛形の `No sources matched for target: xpilot-server` により全体は失敗。サーバー WASM 化を進めて解消するのではなく、段階 5 でネイティブ配信方針に合うビルド対象へ整理する。 |

この環境のネイティブ全体ログは `/tmp/xpilot-browser-native-check.log`、最終 npm 全体ログは `/tmp/xpilot-browser-npm-final.log`。ブラウザのスクリーンショットと動画は `xpilot-client/test-results/browser-driver-1/` と `browser-driver-2/` に保存し、スクリーンショットを目視した。これらの生成物はコミット対象外とする。テストはインストール済み Google Chrome を既定で使い、別の Chromium 実行ファイルは `XPILOT_TEST_BROWSER` で指定できる。

段階 1 の完了条件を照合し、WASM 起動、実 Renderer の表示、同梱フォントとフォールバック、入力・リサイズ、RED→GREEN とネイティブ回帰確認を満たした。起動方法は [ブラウザプレビューの README](../../xpilot-browser/README.md) に記載する。親の既存サブモジュール参照差分は保持し、本体の変更は本体リポジトリのコミットとして管理する。

次は段階 2。配信元の `/xpilot` への WebSocket アダプターと非同期参加を実装する。現時点のドライバーはゲームサーバーへ接続しない。ゲームプレイ、音声・設定、完全なライフサイクル、ネイティブ HTTP 配信・TLS フロントの検証は未完了である。

### 正式統合への計画更新（2026-10-11）

ユーザーの追加指示により、段階 1a を追加した。段階 2〜5 は本体の機能としてすべて実施する。以前の親 npm 全体テストに含まれたサーバー WASM 雛形は本体の成果物ではなく、本体の全体検証はブラウザ npm テストとネイティブ全体テストを組み合わせる。移行だけをもって plan3 完了とはしない。

### 段階 1a の成果物と検証（2026-10-11）

本体の `xpilot-browser/` にアプリ、Makefile、テストと独立した npm lockfile を移した。参照先は本体の `src/`・`vendor/`・`lib/` のみとし、パッケージと生成ランタイムも `xpilot-browser` に改名した。未実装だったライブラリ用 exports はアプリの実際の配布形式に合わせて除いた。旧ディレクトリのビルドコマンドが未導入で失敗する RED を確認した後、新しい場所で SDL3 を再構築し、WASM/Vite ビルドと既存 14 件のテストが成功した。Playwright MCP による倍率 1／2 の実描画、入力、リサイズ、文字ベークを含む。ログ: `/tmp/xpilot-browser-migration-test.log`。本体の README から起動案内へリンクした。段階 1a の完了条件を満たし、段階 2 に進む。

### 段階 2 の成果物（2026-10-11）

ブラウザ WebSocket を既存 `record_transport_t` と `Client_session_step` につないだ。配信元の `/xpilot` と `xpilot-infinity-v1` を使い、バイナリ 1 メッセージを 1 レコードとして受け渡す。送信は bufferedAmount を含めて 1 MiB に制限し、必須レコードの未受理を呼出元へ返す。受信は 256 件／1 MiB、レコードは 65535 bytes を上限とし、型・容量違反は切断する。参加に 15 秒の期限を設け、切断後も Renderer の入力・描画を継続する。

Vitest で通信境界の RED→GREEN を確認。Playwright MCP から Vite の同一オリジン経由で実ネイティブサーバーへ参加し、初期確認レコードを受け取った。同じ試験で切断・サーバー停止後の再試行失敗・描画継続を確認した。単独 E2E ログ: `/tmp/xpilot-browser-session-test.log`。ビルド時 C バージョン情報は既存の screw-up resolver と funcity により生成する。ネイティブゲームの初期化・プレイへの引継ぎは段階 3 で行う。

段階 2 の全ブラウザ試験は当初 19/21 件成功。残り 2 件は既存描画試験がキー送出直後に animation frame の処理結果を待たず判定していたため失敗した。入力結果の実際の座標変化を待つ方式へ修正し、両倍率の試験が成功した（`/tmp/xpilot-browser-render-wait.log`）。通信テスト 6 件と実サーバー参加試験も成功している。段階 3 の開始に際し、ファイル記述子を持たないトランスポートでもゲームループを実行できることと、論理サイズから独立した文字ビットマップを描けることの C テストを追加し、RED を確認した。
