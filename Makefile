# プログラム名
TARGET	= xmkctr

# コンパイラ
CC	= gcc

# コンパイルオプション
CFLAGS	= -Wall -std=c99

# X11ライブラリのリンクオプション
LIBS	= -lX11 -lXi

# ソースファイル
SRCS    = $(TARGET).c

# オブジェクトファイル
OBJS    = $(SRCS:.c=.o)

# デフォルトターゲット
all: $(TARGET)

# 実行ファイルの生成
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LIBS)

# ソースファイルのコンパイル
%.o: %.c
	$(CC) $(CFLAGS) -c $<

# クリーンアップ
clean: rm -f $(OBJS) $(TARGET)

# 実行
run: $(TARGET)
	./$(TARGET)

