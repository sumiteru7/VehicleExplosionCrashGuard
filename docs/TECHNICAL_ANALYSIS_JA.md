# Vehicle Explosion Crash Guard v1.0｜技術解析資料

> 本文中のv0.2.0～v0.2.3は、Nexus公開版v1.0へ至るまでの内部開発バージョン名。公開版v1.0は、最終的なv0.2.3設計を基礎としている。


## Guard A：NULL共有参照

最初の3件で一致したクラッシュ：

```asm
Cyberpunk2077.exe+0x72F311  mov r8,[rdi+50h]
...
Cyberpunk2077.exe+0x72F335  xorps xmm0,xmm0
Cyberpunk2077.exe+0x72F338  movdqa [rsp+20h],xmm0
...
Cyberpunk2077.exe+0x72F346  mov rax,[r8+10h]    ; r8=0でREAD_ADDRESS 0x10
Cyberpunk2077.exe+0x72F34A  test rax,rax
Cyberpunk2077.exe+0x72F34D  je 0x72F374
...
Cyberpunk2077.exe+0x72F374  mov rax,[rsp+28h]
Cyberpunk2077.exe+0x72F379  movaps xmm0,[rsp+20h]
```

`r8==0`時だけ、事前にゼロ初期化されたローカル共有参照を読むゲーム自身の継続点`0x72F374`へ送る。

## Guard B：破損vtableによる仮想呼び出し

第四DMPで確認した別クラッシュ：

- Fault RVA: `Cyberpunk2077.exe+0x2A38A1`
- Instruction: `call qword ptr [rax+8]`
- `r9 = 0x0000027B5C881B60`
- `[r9] = rax = 0xDA443FB80DDAE81B`
- 例外読み取り先: `0xFFFFFFFFFFFFFFFF`
- Thread: GameThread

周辺コード：

```asm
Cyberpunk2077.exe+0x2A3831  call 0x2A38AC
Cyberpunk2077.exe+0x2A3836  test al,al
Cyberpunk2077.exe+0x2A3838  jne  0x2A38A4       ; ゲーム自身の早期終了
...
Cyberpunk2077.exe+0x2A3889  mov rax,[r9]        ; 破損vtableを取得
...
Cyberpunk2077.exe+0x2A38A1  call qword ptr [rax+8]
Cyberpunk2077.exe+0x2A38A4  add rsp,50h
Cyberpunk2077.exe+0x2A38A8  pop rbx
Cyberpunk2077.exe+0x2A38A9  ret
```

`rax`がNULL、低位無効アドレス、カーネル領域、または非正規アドレスの場合だけ、仮想呼び出しを省略して早期終了する。正常なユーザー空間ポインタでは元の仮想呼び出しをそのまま実行する。

Guard Bは今回観測した非正規値`0xDA443FB80DDAE81B`を限定的に遮断する。canonicalだが解放済みのポインタまで推測で広げるための`VirtualQuery`等は入れていない。

## v0.2.0 Guard B復帰クラッシュの照合

最新DMPでは、専用ログ上でGuard Aが2回、Guard Bが1回発動した直後にクラッシュしている。例外コンテキストは次の通り。

```text
Cyberpunk2077.exe base = 0x00007FF67DC10000
Exception RIP          = 0x00007FF67DEB38A4
RVA                    = 0x002A38A4
Exception              = 0xC0000005 / READ 0xFFFFFFFFFFFFFFFF
RSP                    = 0x000000BA303FFB70
```

DMP内のフック設置後コード：

```asm
Cyberpunk2077.exe+0x2A38A1  E9 80 C7 CF FF    jmp SafetyHook stub
Cyberpunk2077.exe+0x2A38A6  90                nop
Cyberpunk2077.exe+0x2A38A7  90                nop
Cyberpunk2077.exe+0x2A38A8  5B                pop rbx
Cyberpunk2077.exe+0x2A38A9  C3                ret
```

元の7バイトは次の2命令だった。

```asm
Cyberpunk2077.exe+0x2A38A1  FF 50 08          call qword ptr [rax+8]
Cyberpunk2077.exe+0x2A38A4  48 83 C4 50       add rsp,50h
```

したがって、v0.2.0が`rip=0x2A38A4`へ変更すると、実行開始位置はSafetyHookの5バイトJMPの4バイト目（`CF`）になる。最新DMPのRIPと実メモリがこの失敗経路を直接確認している。

## SafetyHook v0.7.0での正しいRSP復元

SafetyHook v0.7.0の`Context64`では、`rsp`はフック地点の元RSPを示す読み取り専用値であり、変更しても復帰時の実RSPには反映されない。実RSPを変える場合は`trampoline_rsp`を変更し、その指す先頭に復帰RIPを置く必要がある。

最新DMPでは、フック地点の元RSPを`S`とすると次の配置が確認できる。

```text
[S-0x18] = S                  ; Context::rsp
[S-0x10] = S-0x08             ; Context::trampoline_rsp
[S-0x08] = 0x...2A38A4        ; Context::rip（v0.2.0が設定）
[S+0x50] = 保存RBX
[S+0x58] = 呼び出し元への戻りアドレス
```

`add rsp,50h`を省略して`pop rbx`へ入るには、SafetyHook stubの最後の`ret`が次を同時に満たす必要がある。

```text
retが読む復帰アドレス = Cyberpunk2077.exe+0x2A38A8
ret後の実RSP          = S+0x50
```

そのためv0.2.1では次を行う。

```text
[S+0x48] = Cyberpunk2077.exe+0x2A38A8
trampoline_rsp = S+0x48
rip = Cyberpunk2077.exe+0x2A38A8
```

SafetyHook stubの`ret`が`[S+0x48]`を消費すると、実RSPは`S+0x50`になり、`0x2A38A8: pop rbx`、`0x2A38A9: ret`へ正常に接続する。`[S+0x48]`は終了直前のローカル領域であり、Guard Bが仮想呼び出しを省略して関数を抜ける経路では以後ゲーム側から参照されない。

## 観測設計

RED4ext本体ログを起動判定の一次情報とする。

- Load entered
- Build guard passed / failed
- Guard A/B signature matched / mismatch
- Guard A/B hook installed / failed
- Diagnostic monitor started
- Guard A/B redirect detected + cumulative total

フック上ではファイルI/OやRED4ext loggerを呼ばない。atomicカウンタ更新と`SetEvent`だけを行い、専用監視スレッドがRED4ext本体ログと専用ログへ書く。

専用ログ：

```text
Cyberpunk 2077\red4ext\logs\VehicleExplosionCrashGuard.log
```

起動直後に監視スレッドが新規作成し、各行をflushするため、クラッシュ時もロード・署名・設置・直前までに観測された発動回数が残る。

---

## 2026-08-01再クラッシュ解析とv0.2.2

### セッション1：22:05のアクセス違反

専用ログ：

```text
22:00:23  v0.2.1 Load entered
22:00:23  Build guard passed
22:00:23  Guard A/B signature matched
22:00:23  Guard A/B hook installed
22:04:50  Guard A redirect Total=1
```

Guard B redirectは記録されていない。

DMP：

```text
Exception       = 0xC0000005 / READ
Exception RIP   = 0x00007FF67DBB001A
RAX             = 0x00000005FFFFFFFF
Read address    = 0x0000000600000007
RSP             = 0x00000077436FF580
RCX = R9        = 0x000002413C6ED040
RDX             = RSP+0x20
R8              = RSP+0x30
```

`0x00007FF67DBB001A`はCyberpunk2077.exe本体ではなく、SafetyHookが作成したGuard Bトランポリン内にある。DMP内の実命令は次のとおり。

```asm
0x00007FF67DBB001A  call qword ptr [rax+8]
0x00007FF67DBB001D  add rsp,50h
0x00007FF67DBB0021  jmp Cyberpunk2077.exe+0x2A38A8
```

`rax+8 = 0x0000000600000007`は例外読み取り先と完全一致する。

v0.2.1の判定は、`rax`が`0x10000..0x7FFFFFFFFFFF`に入っていれば正常扱いだった。`0x00000005FFFFFFFF`は数値上この範囲内なのでGuard Bコールバックを通過し、元命令をトランポリンで実行した。

したがって原因は次で確定する。

> Guard Bの復帰処理ではなく、canonicalだがアクセス不能なvtable値を正常扱いした判定漏れ。

Guard Bログが残っていないことも、コールバックがredirect分岐へ入らなかった事実と一致する。

### セッション2：22:10の異常OOM

専用ログ：

```text
22:06:14  v0.2.1 Load entered
22:06:14  Build guard passed
22:06:14  Guard A/B signature matched
22:06:14  Guard A/B hook installed
22:10:48  Guard A redirect Total=1
```

5秒後にOOM assertで終了している。Guard B redirectは記録されていない。

DMP：

```text
Exception       = 0x80000003 / Assert breakpoint
RIP             = Cyberpunk2077.exe+0x2050122
Message         = Out of Memory
Allocation size = 413041423568 = 0x000000602B2FF4D0
Alignment       = 961505456    = 0x00000000394F68B0
Current RSP     = 0x000000602B2FF350
```

スタック上の`current_rsp+0x148`には、Guard Bトランポリンの`call [rax+8]`直後である`0x00007FF67DBB001D`が残っている。

この戻りアドレス位置から、Guard Bフック地点の元RSPを`S`として逆算すると次になる。

```text
S = current_rsp + 0x150 = 0x000000602B2FF4A0
```

Guard B周辺の元コードは、仮想呼び出し前に次を作る。

```asm
lea r8,[rsp+30h]
lea rdx,[rsp+20h]
call qword ptr [rax+8]
```

したがって呼び出し時の第3引数は次。

```text
R8 = S+0x30 = 0x000000602B2FF4D0
```

これはOOMへ渡されたallocation sizeと完全一致する。

```text
OOM allocation size == Guard B call argument R8
0x000000602B2FF4D0 == 0x000000602B2FF4D0
```

正しいGuard B仮想メソッドでは、R8は`rsp+0x30`に置かれた出力先／ローカル構造体ポインタとして渡される。これが確保サイズとして消費されているため、通常のメモリ不足ではなく、別シグネチャの関数へ誤って仮想ディスパッチした結果と判断できる。

一方で、OOMハンドラ到達までに元のSafetyHook Context領域は後続フレームで上書きされており、呼び出し時点の`rax`と`[rax+8]`ターゲットはDMPから完全復元できなかった。

確定範囲：

- Guard Bの同じ仮想呼び出し経路から異常関数へ入っている。
- R8ポインタをallocation sizeとして解釈するABI不一致が起きている。
- Guard Bの安全復帰処理による二次クラッシュではない。
- Guard A発動は先行しているが、例外RIPとクラッシュ経路はGuard Aに一致しない。

未確定範囲：

- 誤ったvtableがアクセス不能／非画像／書き込み可能領域だったか。
- vtable自体は正常な別型で、解放済みオブジェクトが別型へ再利用されたか。
- 実際に呼ばれた仮想関数ターゲットの開始アドレス。

### v0.2.2の最小修正

Guard Aは変更しない。

Guard Bについても次は変更しない。

- Hook RVA `0x2A38A1`
- Safe resume RVA `0x2A38A8`
- 121バイト署名
- SafetyHook v0.7.0
- `[S+0x48]`と`trampoline_rsp`によるRSP復元
- 独立fail-closed設置

変更するのは、仮想呼び出しを許可する前の判定だけ。

v0.2.2では次を検証する。

```text
RCX == R9
RDX == original_rsp+0x20
R8  == original_rsp+0x30
[RCX]を安全に読める
[RCX] == RAX
RAXの領域がMEM_COMMIT / MEM_IMAGE / readable / non-writable
[RAX+8]を安全に読める
[RAX+8]が実行可能なコミット済み領域を指す
[RDX+0] / [RDX+8]のspanが読める
span end >= begin
span size % 0x40 == 0
```

ポインタ読み取りは、`VirtualQuery`による範囲・保護属性確認と、MSVC SEHによる実読み取り保護を組み合わせる。検証に失敗した場合のみ、既存のGuard B安全復帰を実行する。

### 診断強化

Guard B redirect時に、フック内ではatomicへ次だけ保存する。

- rejection reason
- object pointer
- vtable pointer
- function target（読み取れた場合）

監視スレッドが後から文字列化・ファイル出力する。

また、構造検証を通過した仮想関数ターゲットを重複除外で最大8件記録する。これは2件目が再発した場合に、正常ターゲット群と異常ターゲットを比較するための診断情報であり、動的allowlistとしては使わない。

### v0.2.2の保証範囲

- 22:05 DMPの`rax=0x00000005FFFFFFFF`は、vtable領域検証で確実にredirect対象になる。
- 22:10 DMPの誤ディスパッチが、非画像vtable・書き込み可能vtable・読取不能slot・非実行target・呼出不変条件破綻のいずれかならredirect対象になる。
- 22:10 DMPが「構造上は完全に正常な別型vtable」を指していた場合、現資料だけで安全な静的allowlistは作れないため、v0.2.2だけで遮断できるとは断定しない。


---

## 2026-08-02｜v0.2.2性能問題とv0.2.3再設計

### v0.2.2の問題

実機ログでは、通常プレイ開始後の短時間にGuard Bの正常ターゲットが複数種類観測された。Guard Bは車両爆発時だけでなく高頻度の共通処理で通過する。

v0.2.2は正常な1回ごとに複数の`VirtualQuery`、SEH保護付き読み取り、descriptor検証、正常ターゲット観測用atomic配列走査を行っていた。この設計はクラッシュ時だけの安全処理ではなく、正常経路へ高コストを常駐させており、目視できるFPS低下を起こした。

v0.2.2は通常使用版として撤回する。

### v0.2.3の原則

- v0.2.1の軽量正常経路を基準に戻す。
- 起動時に一度だけPEヘッダを解析する。
- 正常時はOS API、SEH、ログ、atomic、探索用更新を行わない。
- DMPで確認できた範囲外ポインタだけを安価に遮断する。
- 2件目のOOMを理由に、構造上有効なゲーム本体vtableまで推測で遮断しない。

### 起動時処理

Cyberpunk2077.exeのPEセクションから次を固定長配列へ保存する。

- 読み取り可能、非書き込み、非実行、非discardableセクション
- 読み取り可能、実行可能、非discardableセクション

範囲初期化に失敗した場合はGuard Bを設置しない。

### Guard B正常経路

```text
1. RAXのポインタ整列確認
2. RCX/R9、RDX/RSP+0x20、R8/RSP+0x30の整数比較
3. RAX..RAX+0x0Fが読み取り専用PEセクション内か比較
4. target = [RAX+8] を1回読み取り
5. targetが実行可能PEセクション内か比較
6. 成立時は即return
```

正常経路では`VirtualQuery`、SEH、descriptor読み取り、正常ターゲット記録を行わない。

### 保証範囲

- `0xDA443FB80DDAE81B`はPEセクション外として遮断する。
- `0x00000005FFFFFFFF`は非整列かつPEセクション外として遮断する。
- vtable内の関数ポインタがゲーム本体の実行可能セクション外なら遮断する。
- vtableと関数ターゲットの両方が構造上有効なゲーム本体内アドレスだった場合は通過させる。

最後のケースで異常OOMが再発した場合は、実ターゲットを追加資料で確定した後、そのターゲットだけを狭くGuardする。性能を犠牲にする汎用ポインタ生存判定へは戻さない。
