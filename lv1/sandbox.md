# sandbox.c の処理フロー解説

## 1. sandbox の概要

### 関数シグネチャ
```c
int sandbox(void (*f)(void), unsigned int timeout, bool verbose);
```

### 目的
- 危険な可能性のある関数を安全に実行・評価
- 関数の「良い」「悪い」を判定（クラッシュ、タイムアウト、異常終了の検出）
- 子プロセスで隔離実行することで親プロセスを保護
- タイムアウト機能による無限ループ対策

### 戻り値
- 1: 良い関数（正常終了、exit code 0）
- 0: 悪い関数（シグナル終了、非0終了、タイムアウト）
- -1: エラー（sandbox自体のエラー）

## 2. 実際のコード（修正版）

```c
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <string.h>

void alarm_handler(int sig)
{
    (void)sig;
    // Empty handler - just to interrupt waitpid
}

int sandbox(void (*f)(void), unsigned int timeout, bool verbose)
{
    struct sigaction sa, old_sa;
    pid_t pid;
    int status;
    
    // Fork child process
    pid = fork();
    if (pid == -1)
        return (-1);
    
    if (pid == 0)
    {
        // Child process: execute the function
        f();
        exit(0);
    }
    
    // Parent process: set up signal handler
    sa.sa_handler = alarm_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGALRM, &sa, &old_sa);
    
    // Set alarm
    alarm(timeout);
    
    // Wait for child
    if (waitpid(pid, &status, 0) == -1)
    {
        if (errno == EINTR)
        {
            // Timeout occurred
            kill(pid, SIGKILL);
            waitpid(pid, NULL, 0);
            alarm(0);  // Cancel any remaining alarm
            sigaction(SIGALRM, &old_sa, NULL);  // Restore old handler
            if (verbose)
                printf("Bad function: timed out after %u seconds\n", timeout);
            return (0);
        }
        alarm(0);  // Cancel alarm on error
        sigaction(SIGALRM, &old_sa, NULL);
        return (-1);
    }
    
    // Cancel alarm after successful wait
    alarm(0);
    sigaction(SIGALRM, &old_sa, NULL);
    
    // Check exit status
    if (WIFEXITED(status))
    {
        if (WEXITSTATUS(status) == 0)
        {
            if (verbose)
                printf("Nice function!\n");
            return (1);
        }
        else
        {
            if (verbose)
                printf("Bad function: exited with code %d\n", WEXITSTATUS(status));
            return (0);
        }
    }
    
    if (WIFSIGNALED(status))
    {
        int sig = WTERMSIG(status);
        if (verbose)
            printf("Bad function: %s\n", strsignal(sig));
        return (0);
    }
    
    return (-1);
}
```

## 3. 処理フロー詳細

### ステップ1: プロセスのフォーク
```c
pid = fork();
if (pid == -1)
    return (-1);
```
- 子プロセスを作成して危険な関数を隔離実行
- fork失敗時は即座に-1を返す

### ステップ2: 子プロセスの処理
```c
if (pid == 0)
{
    f();
    exit(0);
}
```
- 渡された関数`f`を実行
- 正常に戻った場合は`exit(0)`で終了
- クラッシュした場合はシグナルで終了

### ステップ3: 親プロセスでのシグナル設定
```c
sa.sa_handler = alarm_handler;
sa.sa_flags = 0;
sigemptyset(&sa.sa_mask);
sigaction(SIGALRM, &sa, &old_sa);
```
- `SIGALRM`用のハンドラを設定
- 元のハンドラを`old_sa`に保存

### ステップ4: タイマー設定と待機
```c
alarm(timeout);
if (waitpid(pid, &status, 0) == -1)
```
- `alarm()`でタイムアウトを設定
- `waitpid()`で子プロセスの終了を待つ

### ステップ5: タイムアウト処理
```c
if (errno == EINTR)
{
    kill(pid, SIGKILL);
    waitpid(pid, NULL, 0);
    // クリーンアップと出力
}
```
- `SIGALRM`により`waitpid`が中断された場合
- 子プロセスを強制終了（`SIGKILL`）
- ゾンビプロセス防止のため再度`waitpid`

### ステップ6: 終了状態の判定
```c
if (WIFEXITED(status))  // 正常終了
if (WIFSIGNALED(status))  // シグナルで終了
```
- 終了コードやシグナル番号を取得
- verboseモードで適切なメッセージを出力

## 4. 処理フローの視覚的説明

### メイン処理フロー
```
sandbox開始
    ↓
fork() でプロセス分岐
    ├─ 失敗 → return -1
    │
    ├─ 子プロセス (pid == 0)
    │   ├─ f() を実行
    │   └─ exit(0)
    │
    └─ 親プロセス
        ├─ SIGALRMハンドラ設定
        ├─ alarm(timeout) 設定
        ├─ waitpid() で待機
        │   ├─ 正常終了 → 終了状態チェック
        │   └─ EINTR → タイムアウト処理
        ├─ alarm(0) でタイマーキャンセル
        ├─ 元のハンドラ復元
        └─ 結果を返す (1/0/-1)
```

### タイムアウトの動作
```
時間軸 →
────────────────────────────────────>
0秒                              timeout秒
├─ alarm(timeout)                    │
├─ waitpid()開始                     │
│                                    ├─ SIGALRM発生
│                                    ├─ waitpid()がEINTRで中断
│                                    ├─ kill(pid, SIGKILL)
│                                    └─ "timed out" 出力
```

### 終了パターンの分類
```
関数f()の動作結果
    │
    ├─ 正常に戻る → exit(0) → WIFEXITED && code==0 → Nice! (1)
    │
    ├─ exit(n) (n≠0) → WIFEXITED && code!=0 → Bad (0)
    │
    ├─ クラッシュ (SIGSEGV等) → WIFSIGNALED → Bad (0)
    │
    └─ 無限ループ → タイムアウト → SIGKILL → Bad (0)
```

## 5. 具体的な処理例

### 例1: 正常な関数
```c
void good_function(void) {
    printf("Hello\n");
    // 正常に戻る
}

result = sandbox(good_function, 5, true);
// 出力: "Nice function!"
// 戻り値: 1
```

### 例2: セグメンテーションフォルト
```c
void bad_segfault(void) {
    int *p = NULL;
    *p = 42;  // SIGSEGV
}

result = sandbox(bad_segfault, 5, true);
// 出力: "Bad function: Segmentation fault"
// 戻り値: 0
```

### 例3: タイムアウト
```c
void bad_infinite(void) {
    while (1) {
        // 無限ループ
    }
}

result = sandbox(bad_infinite, 2, true);
// 出力: "Bad function: timed out after 2 seconds"
// 戻り値: 0
```

### 例4: 非ゼロ終了
```c
void bad_exit(void) {
    exit(42);
}

result = sandbox(bad_exit, 5, true);
// 出力: "Bad function: exited with code 42"
// 戻り値: 0
```

## 6. エラーハンドリングの詳細

### 実装されているエラーチェック

1. **フォークエラー**:
   ```c
   if (pid == -1)
       return (-1);
   ```

2. **waitpidエラー（タイムアウト以外）**:
   ```c
   if (waitpid(pid, &status, 0) == -1)
   {
       if (errno == EINTR) { /* タイムアウト処理 */ }
       // その他のエラー
       alarm(0);
       sigaction(SIGALRM, &old_sa, NULL);
       return (-1);
   }
   ```

3. **クリーンアップの保証**:
   - すべてのパスで`alarm(0)`を呼び出し
   - 元のシグナルハンドラを復元
   - ゾンビプロセスを防ぐため必ず`waitpid`

## 7. 重要なポイント

### シグナル処理
1. **空のハンドラの意味**: `waitpid`を中断させるためだけに存在
2. **EINTR の利用**: シグナルによる中断を検出
3. **元のハンドラの復元**: 他のコードへの影響を防ぐ

### プロセス管理
- **fork による隔離**: 親プロセスを保護
- **SIGKILL の使用**: 確実な終了を保証
- **二重waitpid**: ゾンビプロセス防止

### タイミング制御
- **alarm()**: 簡潔なタイムアウト実装
- **即座のキャンセル**: 不要になったらすぐに`alarm(0)`

### verboseモードの出力
- 各終了パターンに応じた明確なメッセージ
- `strsignal()`を使用してシグナル名を表示
