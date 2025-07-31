# picoshell.c の処理フロー解説

## 1. picoshell の概要

### 関数シグネチャ
```c
int picoshell(char **cmds[])
```

### 目的
- シェルのパイプライン機能を実装
- 複数のコマンドを連結して実行（cmd1 | cmd2 | cmd3）
- 各コマンドの出力を次のコマンドの入力に接続
- UNIXシェルのパイプ処理の基本的な実装例

### 戻り値
- 成功時: 0
- エラー時: 1

## 2. 実際のコード

```c
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>

int picoshell(char **cmds[])
{
    pid_t   pid;
    int     pipefd[2];
    int     prev_fd;
    int     status;
    int     exit_code;
    int     i;
    
    prev_fd = -1;
    exit_code = 0;
    i = 0;
    
    while (cmds[i])
    {
        if (cmds[i + 1] && pipe(pipefd))
            return (1);
        
        pid = fork();
        if (pid == -1)
        {
            if (cmds[i + 1])
            {
                close(pipefd[0]);
                close(pipefd[1]);
            }
            return (1);
        }
        
        if (pid == 0)  // 子プロセス
        {
            if (prev_fd != -1)
            {
                if (dup2(prev_fd, STDIN_FILENO) == -1)
                    exit(1);
                close(prev_fd);
            }
            if (cmds[i + 1])
            {
                close(pipefd[0]);
                if (dup2(pipefd[1], STDOUT_FILENO) == -1)
                    exit(1);
                close(pipefd[1]);
            }
            execvp(cmds[i][0], cmds[i]);
            exit(1);
        }
        
        // 親プロセス
        if (prev_fd != -1)
            close(prev_fd);
        if (cmds[i + 1])
        {
            close(pipefd[1]);
            prev_fd = pipefd[0];
        }
        i++;
    }
    
    while (wait(&status) != -1)
    {
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
            exit_code = 1;
    }
    
    return (exit_code);
}
```

## 3. 処理フロー詳細

### ステップ1: 初期化
```c
prev_fd = -1;      // 前のパイプの読み取り端
exit_code = 0;     // 最終的な終了コード
i = 0;             // コマンドのインデックス
```

### ステップ2: コマンドループ
```c
while (cmds[i])
```
- NULLに到達するまで各コマンドを処理
- 各イテレーションで1つのコマンドを実行

### ステップ3: パイプ作成（必要な場合のみ）
```c
if (cmds[i + 1] && pipe(pipefd))
    return (1);
```
- 次のコマンドが存在する場合のみパイプを作成
- 最後のコマンドではパイプ不要
- パイプ作成失敗時は即座に1を返す

### ステップ4: プロセスのフォーク
```c
pid = fork();
if (pid == -1)
{
    if (cmds[i + 1])
    {
        close(pipefd[0]);
        close(pipefd[1]);
    }
    return (1);
}
```
- fork失敗時は作成したパイプを閉じてエラー返却

### ステップ5: 子プロセスの処理
```c
if (pid == 0)  // 子プロセス
{
    // 入力のリダイレクト（最初のコマンド以外）
    if (prev_fd != -1)
    {
        if (dup2(prev_fd, STDIN_FILENO) == -1)
            exit(1);
        close(prev_fd);
    }
    
    // 出力のリダイレクト（最後のコマンド以外）
    if (cmds[i + 1])
    {
        close(pipefd[0]);
        if (dup2(pipefd[1], STDOUT_FILENO) == -1)
            exit(1);
        close(pipefd[1]);
    }
    
    execvp(cmds[i][0], cmds[i]);
    exit(1);
}
```

### ステップ6: 親プロセスの処理
```c
// 親プロセス
if (prev_fd != -1)
    close(prev_fd);
if (cmds[i + 1])
{
    close(pipefd[1]);
    prev_fd = pipefd[0];
}
i++;
```

### ステップ7: 全子プロセスの待機
```c
while (wait(&status) != -1)
{
    if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
        exit_code = 1;
}
```

## 4. 処理フローの視覚的説明

### メイン処理フロー
```
picoshell開始
    ↓
初期化 (prev_fd = -1, exit_code = 0, i = 0)
    ↓
┌─ コマンドループ (while cmds[i])
│   │
│   ├─ 次のコマンドあり？
│   │   └─ YES → pipe(pipefd)
│   │       └─ 失敗 → return 1
│   │
│   ├─ fork()でプロセス分岐
│   │   ├─ 失敗 → パイプ閉じて return 1
│   │   │
│   │   ├─ 子プロセス (pid == 0)
│   │   │   ├─ prev_fd != -1 → stdin接続
│   │   │   ├─ cmds[i+1] != NULL → stdout接続
│   │   │   └─ execvp() → exit(1)
│   │   │
│   │   └─ 親プロセス
│   │       ├─ prev_fd閉じる
│   │       ├─ 次のコマンドあり → prev_fd更新
│   │       └─ i++
│   │
│   └─ 次のコマンドへ
│
└─ 全子プロセス待機
    └─ エラーチェック → return exit_code
```

### パイプラインの動作例（3コマンド）
```
cmd1 | cmd2 | cmd3

[イテレーション1: cmd1]
- パイプ作成: pipe1[0], pipe1[1]
- fork() → 子プロセス1
  - stdout → pipe1[1]にリダイレクト
  - execvp(cmd1)
- 親: prev_fd = pipe1[0]

[イテレーション2: cmd2]
- パイプ作成: pipe2[0], pipe2[1]
- fork() → 子プロセス2
  - stdin ← pipe1[0]からリダイレクト
  - stdout → pipe2[1]にリダイレクト
  - execvp(cmd2)
- 親: prev_fd = pipe2[0]

[イテレーション3: cmd3]
- パイプ作成なし（最後のコマンド）
- fork() → 子プロセス3
  - stdin ← pipe2[0]からリダイレクト
  - execvp(cmd3)
- 親: 待機フェーズへ
```

## 5. 具体的な処理例

### 例1: `ls | grep picoshell`
```c
char **cmds[] = {
    (char *[]){"/bin/ls", NULL},
    (char *[]){"grep", "picoshell", NULL},
    NULL
};
picoshell(cmds);
```

#### 実行時の動作:
1. **i=0 (ls)**:
   - `pipe(pipefd)` → pipefd[0]=3, pipefd[1]=4
   - `fork()` → 子プロセス1
   - 子: `dup2(4, 1)`, `execvp("ls", ...)`
   - 親: `close(4)`, `prev_fd = 3`

2. **i=1 (grep)**:
   - 次のコマンドなし → パイプ作成しない
   - `fork()` → 子プロセス2
   - 子: `dup2(3, 0)`, `execvp("grep", ...)`
   - 親: `close(3)`

3. **待機フェーズ**:
   - 両子プロセスの終了を待つ

### 例2: `echo 'squalala' | cat | sed 's/a/b/g'`
```c
char **cmds[] = {
    (char *[]){"echo", "squalala", NULL},
    (char *[]){"cat", NULL},
    (char *[]){"sed", "s/a/b/g", NULL},
    NULL
};
picoshell(cmds);
```

#### データフロー:
```
echo 'squalala'     cat             sed 's/a/b/g'
     stdout ──→ stdin/stdout ──→ stdin
                                     ↓
                                 squblblb
```

## 6. エラーハンドリングの詳細

### 実装されているエラーチェック

1. **パイプ作成エラー**:
   ```c
   if (cmds[i + 1] && pipe(pipefd))
       return (1);
   ```

2. **フォークエラー**:
   ```c
   if (pid == -1)
   {
       if (cmds[i + 1])
       {
           close(pipefd[0]);
           close(pipefd[1]);
       }
       return (1);
   }
   ```

3. **dup2エラー**:
   ```c
   if (dup2(prev_fd, STDIN_FILENO) == -1)
       exit(1);
   ```

4. **子プロセスの実行エラー**:
   ```c
   while (wait(&status) != -1)
   {
       if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
           exit_code = 1;
   }
   ```

## 7. 重要なポイント

### ファイルディスクリプタの管理
1. **使用済みfdは必ず閉じる**: リーク防止
2. **dup2後は元のfdも閉じる**: 重複を避ける
3. **親プロセスは不要な端を閉じる**: デッドロック防止

### パイプの接続パターン
- **最初のコマンド**: stdoutのみリダイレクト
- **中間のコマンド**: stdinとstdoutの両方をリダイレクト
- **最後のコマンド**: stdinのみリダイレクト

### プロセスの同期
- `wait()`を使用して全子プロセスの終了を待機
- いずれかのコマンドが失敗した場合、exit_code = 1

### メモリ効率
- PIDの配列を使わず、シンプルなループで実装
- 必要なパイプのみを作成（最後のコマンドではパイプ不要）
