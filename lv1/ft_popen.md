# ft_popen.c の処理フロー解説

## 1. ft_popen の概要

### 関数シグネチャ
```c
int ft_popen(const char *file, char *const argv[], char type)
```

### 目的
- 外部コマンドを実行し、その入出力をパイプ経由で制御
- 標準ライブラリの`popen()`の簡易実装
- プロセス間通信（IPC）の基本的な実装例

### 戻り値
- 成功時: 使用可能なファイルディスクリプタ
- エラー時: -1

## 2. 実際のコード

```c
#include <unistd.h>
#include <stdlib.h>

int ft_popen(const char *file, char *const argv[], char type)
{
    if (!file || !argv || (type != 'r' && type != 'w'))
        return -1;
    
    int fd[2];
    if (pipe(fd) == -1)
        return -1;
    
    pid_t pid = fork();
    if (pid == -1)
    {
        close(fd[0]);
        close(fd[1]);
        return -1;
    }
    
    if (pid == 0)  // 子プロセス
    {
        if (type == 'r')
        {
            dup2(fd[1], STDOUT_FILENO);
            close(fd[0]);
            close(fd[1]);
            execvp(file, argv);
            exit(-1);
        }
        if (type == 'w')
        {
            dup2(fd[0], STDIN_FILENO);
            close(fd[0]);
            close(fd[1]);
            execvp(file, argv);
            exit(-1);
        }
    }
    
    // 親プロセス
    if (type == 'r')
    {
        close(fd[1]);
        return (fd[0]);
    }
    if (type == 'w')
    {
        close(fd[0]);
        return (fd[1]);
    }
    
    return -1;
}
```

## 3. 処理フロー詳細

### ステップ1: パラメータ検証
```c
if (!file || !argv || (type != 'r' && type != 'w'))
    return -1;
```
- `file`がNULLでないか確認
- `argv`がNULLでないか確認
- `type`が'r'または'w'であることを確認

### ステップ2: パイプ作成とエラーチェック
```c
int fd[2];
if (pipe(fd) == -1)
    return -1;
```
- `fd[0]`: 読み取り端（read end）
- `fd[1]`: 書き込み端（write end）
- パイプ作成失敗時は即座に-1を返す

### ステップ3: プロセスのフォークとエラーチェック
```c
pid_t pid = fork();
if (pid == -1)
{
    close(fd[0]);
    close(fd[1]);
    return -1;
}
```
- fork失敗時はパイプのファイルディスクリプタを閉じてから-1を返す
- メモリリークを防ぐため、必ず両端を閉じる

### ステップ4: 子プロセスの処理
```c
if (pid == 0)  // 子プロセス
{
    if (type == 'r')
    {
        dup2(fd[1], STDOUT_FILENO);  // 標準出力をパイプに接続
        close(fd[0]);                // 読み取り端は不要
        close(fd[1]);                // dup2後は元のfdも不要
        execvp(file, argv);          // コマンド実行
        exit(-1);                    // execvp失敗時の終了
    }
    if (type == 'w')
    {
        dup2(fd[0], STDIN_FILENO);   // 標準入力をパイプに接続
        close(fd[0]);                // dup2後は元のfdも不要
        close(fd[1]);                // 書き込み端は不要
        execvp(file, argv);          // コマンド実行
        exit(-1);                    // execvp失敗時の終了
    }
}
```

### ステップ5: 親プロセスの処理
```c
// 親プロセス
if (type == 'r')
{
    close(fd[1]);     // 書き込み端は不要
    return (fd[0]);   // 読み取り端を返す
}
if (type == 'w')
{
    close(fd[0]);     // 読み取り端は不要
    return (fd[1]);   // 書き込み端を返す
}
```

## 4. 処理フローの視覚的説明

### メイン処理フロー
```
ft_popen開始
    ↓
パラメータ検証 (!file || !argv || (type != 'r' && type != 'w'))
    ├─ 無効 → return -1
    └─ 有効 ↓
        pipe(fd)でパイプ作成
            ├─ 失敗 → return -1
            └─ 成功 ↓
                fork()でプロセス分岐
                    ├─ 失敗 → close(fd[0]), close(fd[1]), return -1
                    └─ 成功 ↓
                        ├─ 子プロセス (pid == 0)
                        │     ├─ type == 'r' → stdout接続処理
                        │     └─ type == 'w' → stdin接続処理
                        └─ 親プロセス
                              ├─ type == 'r' → close(fd[1]), return fd[0]
                              └─ type == 'w' → close(fd[0]), return fd[1]
```

### 'r'モード（読み取り）の処理フロー
```
子プロセス側:
1. dup2(fd[1], STDOUT_FILENO) - 標準出力をパイプに接続
2. close(fd[0]) - 読み取り端を閉じる
3. close(fd[1]) - dup2後は元のfdも不要
4. execvp(file, argv) - コマンドを実行
5. exit(-1) - execvp失敗時のみ到達

親プロセス側:
1. close(fd[1]) - 書き込み端を閉じる
2. return fd[0] - 読み取り端を返す
```

### 'w'モード（書き込み）の処理フロー
```
子プロセス側:
1. dup2(fd[0], STDIN_FILENO) - 標準入力をパイプに接続
2. close(fd[0]) - dup2後は元のfdも不要
3. close(fd[1]) - 書き込み端を閉じる
4. execvp(file, argv) - コマンドを実行
5. exit(-1) - execvp失敗時のみ到達

親プロセス側:
1. close(fd[0]) - 読み取り端を閉じる
2. return fd[1] - 書き込み端を返す
```

## 5. 具体的な処理例

### 例1: `ls`コマンドの出力を読む
```c
int fd = ft_popen("ls", (char *const[]){"ls", NULL}, 'r');
```

#### 実行時の動作:
1. **パラメータ検証**: すべて有効なので続行
2. **パイプ作成**: `pipe(fd)` → fd[0]=3, fd[1]=4（例）
3. **フォーク**: 親プロセスと子プロセスに分岐
4. **子プロセス**:
   - `dup2(4, 1)`: 標準出力(1)をfd[1](4)にリダイレクト
   - `close(3)`, `close(4)`: 不要なfd閉じる
   - `execvp("ls", ...)`: lsコマンドに置換
   - lsの出力はパイプに流れる
5. **親プロセス**:
   - `close(4)`: 書き込み端閉じる
   - `return 3`: 読み取り端を返す

### 例2: `grep`コマンドへ入力を送る
```c
int fd = ft_popen("grep", (char *const[]){"grep", "test", NULL}, 'w');
write(fd, "test line\nother line\n", 21);
```

#### 実行時の動作:
1. **パラメータ検証**: すべて有効なので続行
2. **パイプ作成**: `pipe(fd)` → fd[0]=3, fd[1]=4（例）
3. **フォーク**: 親プロセスと子プロセスに分岐
4. **子プロセス**:
   - `dup2(3, 0)`: 標準入力(0)をfd[0](3)にリダイレクト
   - `close(3)`, `close(4)`: 不要なfd閉じる
   - `execvp("grep", ...)`: grepコマンドに置換
   - grepはパイプから入力を読む
5. **親プロセス**:
   - `close(3)`: 読み取り端閉じる
   - `return 4`: 書き込み端を返す

## 6. データフローの図解

### 'r'モード（コマンド出力を読む）
```
子プロセス(ls)                親プロセス
    stdout ─┐                     │
            ↓                     │
         fd[1] ──→ パイプ ──→ fd[0]
                                  ↓
                              読み取り可能
```

### 'w'モード（コマンドへ入力を送る）
```
親プロセス                    子プロセス(grep)
    書き込み                      │
       ↓                         │
    fd[1] ──→ パイプ ──→ fd[0]   │
                            ↓     │
                         stdin ───┘
```

## 7. エラーハンドリングの詳細

### 実装されているエラーチェック
1. **パラメータ検証**:
   - `file`がNULLの場合
   - `argv`がNULLの場合
   - `type`が'r'でも'w'でもない場合

2. **システムコールのエラー**:
   - `pipe()`失敗時: -1を返す
   - `fork()`失敗時: パイプを閉じて-1を返す

3. **子プロセスでのエラー**:
   - `execvp()`失敗時: `exit(-1)`で終了

### エラー時のリソース管理
- fork失敗時は必ず両端のファイルディスクリプタを閉じる
- これによりファイルディスクリプタのリークを防ぐ

## 8. 重要なポイント

### ファイルディスクリプタのリーク防止
1. **子プロセス**: dup2後は両端とも必ずclose
2. **親プロセス**: 使わない端は必ずclose
3. **エラー時**: 開いたfdは必ず閉じる

### fork()とexecvp()の組み合わせ
- `fork()`: 現在のプロセスの完全なコピーを作成
- `execvp()`: プロセスを新しいプログラムで置き換え
- この組み合わせでUNIXの新プロセス起動を実現

### パイプの単方向性
- パイプは単方向通信のみ
- 双方向通信には2つのパイプが必要
