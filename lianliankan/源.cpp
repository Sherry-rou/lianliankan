﻿#include <graphics.h>
#include <conio.h>
#include <time.h>
#include <vector>
#include <string>
#include <algorithm>
#include <windows.h>
#include <cstdlib>
#include <cstdio>

// 窗口配置
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define BOARD_OFFSET_X 50
#define BOARD_OFFSET_Y 120

// 棋盘配置
#define ROW 6
#define COL 8
#define BLOCK_SIZE 60

// 游戏状态枚举
enum GameState {
	STATE_START,    // 开始界面
	STATE_GAME,     // 游戏中
	STATE_PAUSE,    // 暂停界面
	STATE_WIN,      // 胜利界面
	STATE_LOSE      // 失败界面
};

// 方块数据结构：存储数值、状态、坐标等核心信息
struct Block {
	int value;          // 方块数值（对应图案）
	bool eliminated;    // 是否已消除
	bool selected;      // 是否被选中
	int x, y;           // 像素坐标
	int row, col;       // 棋盘行列坐标
};

// 关卡配置：不同关卡的时间、得分、道具规则
struct LevelConfig {
	int time_limit;     // 时间限制(秒)
	int base_score;     // 基础得分
	int combo_add;      // 连击加分
	int prop_hint;      // 提示道具数量
	int prop_shuffle;   // 洗牌道具数量
};

// 全局变量
Block board[ROW][COL];                // 棋盘数据
GameState current_state = STATE_START;// 当前游戏状态
LevelConfig current_level;            // 当前关卡配置
int score = 0;                        // 玩家得分
int combo = 0;                        // 连击数
int remaining_time = 0;               // 剩余时间
clock_t start_time;                   // 游戏开始时间戳
bool is_paused = false;               // 暂停标记
std::pair<int, int> selected_block = { -1, -1 }; // 选中的方块坐标
std::vector<std::pair<int, int>> path;           // 方块连接路径
IMAGE images[10];                     // 图片数组（对应value 1-10）

// Fisher-Yates洗牌算法：打乱数组顺序（保证随机公平）
void manual_shuffle(std::vector<int>& vec) {
	int n = vec.size();
	for (int i = n - 1; i > 0; --i) {
		int j = rand() % (i + 1);
		std::swap(vec[i], vec[j]);
	}
}

// 初始化关卡配置：不同关卡难度递进
void init_level_config(int level = 1) {
	switch (level) {
	case 1:
		current_level = { 120, 10, 5, 3, 2 }; // 简单：120秒，基础分10，连击+5，提示3个，洗牌2个
		break;
	case 2:
		current_level = { 90, 15, 8, 2, 1 };  // 中等：90秒，基础分15，连击+8，提示2个，洗牌1个
		break;
	case 3:
		current_level = { 60, 20, 10, 1, 0 };  // 困难：60秒，基础分20，连击+10，提示1个，洗牌0个
		break;
	default:
		current_level = { 120, 10, 5, 3, 2 };
	}
	remaining_time = current_level.time_limit;
	score = 0;
	combo = 0;
}

// 初始化棋盘：生成成对数值+随机洗牌，重置方块状态
void init_board() {
	// 初始化方块基础属性
	for (int i = 0; i < ROW; i++) {
		for (int j = 0; j < COL; j++) {
			board[i][j].row = i;
			board[i][j].col = j;
			board[i][j].x = BOARD_OFFSET_X + j * BLOCK_SIZE;
			board[i][j].y = BOARD_OFFSET_Y + i * BLOCK_SIZE;
			board[i][j].eliminated = false;
			board[i][j].selected = false;
		}
	}

	// 生成成对数值（保证每个数值有两个，可配对消除）
	std::vector<int> values;
	int total_pairs = (ROW * COL) / 2;
	for (int i = 0; i < total_pairs; i++) {
		int val = (i % 10) + 1; // 限制数值范围1-10，对应不同图案
		values.push_back(val);
		values.push_back(val);
	}

	// 洗牌：打乱数值顺序
	srand((unsigned int)time(NULL));
	manual_shuffle(values);

	// 为棋盘方块赋值
	int idx = 0;
	for (int i = 0; i < ROW; i++) {
		for (int j = 0; j < COL; j++) {
			board[i][j].value = values[idx++];
		}
	}

	selected_block = { -1, -1 };
	path.clear();
}

// 检查坐标是否在棋盘有效范围内
bool is_in_board(int row, int col) {
	return row >= 0 && row < ROW && col >= 0 && col < COL;
}

// 检查位置是否为空（已消除/棋盘外）：连通检测的基础判断
bool is_empty(int row, int col) {
	if (!is_in_board(row, col)) return true; // 棋盘外视为空（支持外部绕路）
	return board[row][col].eliminated;
}

// 检查直线连通：同行/同列且无遮挡（核心连通规则1）
bool check_straight(int r1, int c1, int r2, int c2) {
	if (r1 == r2 && c1 == c2) return false;

	// 水平方向检测
	if (r1 == r2) {
		int min_col = min(c1, c2);
		int max_col = max(c1, c2);
		for (int c = min_col + 1; c < max_col; c++) {
			if (!is_empty(r1, c)) return false;
		}
		return true;
	}

	// 垂直方向检测
	if (c1 == c2) {
		int min_row = min(r1, r2);
		int max_row = max(r1, r2);
		for (int r = min_row + 1; r < max_row; r++) {
			if (!is_empty(r, c1)) return false;
		}
		return true;
	}

	return false;
}

// 检查单拐点连通：一个拐角，也就是一个 L 形路径（核心连通规则2）
bool check_one_corner(int r1, int c1, int r2, int c2) {
	// 拐点 (r1, c2)
	if (is_empty(r1, c2) &&
		check_straight(r1, c1, r1, c2) &&
		check_straight(r1, c2, r2, c2)) {
		return true;
	}
	// 拐点 (r2, c1)
	if (is_empty(r2, c1) &&
		check_straight(r1, c1, r2, c1) &&
		check_straight(r2, c1, r2, c2)) {
		return true;
	}
	return false;
}

// 检查双拐点连通：两个拐角 → 支持棋盘外左右/上下绕路
bool check_two_corner(int r1, int c1, int r2, int c2) {
	// 上下绕路（顶部 / 底部）
	// 顶部绕路
	if (check_straight(r1, c1, -1, c1) &&
		check_straight(-1, c1, -1, c2) &&
		check_straight(-1, c2, r2, c2)) {
		return true;
	}
	// 底部绕路
	if (check_straight(r1, c1, ROW, c1) &&
		check_straight(ROW, c1, ROW, c2) &&
		check_straight(ROW, c2, r2, c2)) {
		return true;  
	}

	// 方案左右绕路（左侧 / 右侧）

	// 左侧绕路
	if (check_straight(r1, c1, r1, -1) &&
		check_straight(r1, -1, r2, -1) &&
		check_straight(r2, -1, r2, c2)) {
		return true;
	}
	// 右侧绕路
	if (check_straight(r1, c1, r1, COL) &&
		check_straight(r1, COL, r2, COL) &&
		check_straight(r2, COL, r2, c2)) {
		return true;
	}

	return false;
}

// 核心连通检测：直线→单拐点→双拐点（层层递进）
bool is_connected(int r1, int c1, int r2, int c2) {
	path.clear();
	// 基础校验
	if (r1 == r2 && c1 == c2) return false;
	if (!is_in_board(r1, c1) || !is_in_board(r2, c2)) return false;
	if (board[r1][c1].value != board[r2][c2].value) return false;
	if (board[r1][c1].eliminated || board[r2][c2].eliminated) return false;

	// 1. 直线
	if (check_straight(r1, c1, r2, c2)) return true;
	// 2. 单拐角
	if (check_one_corner(r1, c1, r2, c2)) return true;
	// 3. 双拐角（外部绕路）
	if (check_two_corner(r1, c1, r2, c2)) return true;

	return false;
}

// 处理方块点击：选中/取消选中/消除逻辑（游戏核心交互）
void handle_block_click(int mouse_x, int mouse_y) {
	int click_row = -1, click_col = -1;

	// 定位点击的方块
	for (int i = 0; i < ROW; i++) {
		for (int j = 0; j < COL; j++) {
			if (!board[i][j].eliminated &&
				mouse_x >= board[i][j].x && mouse_x <= board[i][j].x + BLOCK_SIZE &&
				mouse_y >= board[i][j].y && mouse_y <= board[i][j].y + BLOCK_SIZE) {
				click_row = i;
				click_col = j;
				break;
			}
		}
		if (click_row != -1) break;
	}

	if (click_row == -1) return;

	if (selected_block.first == -1) {
		// 首次选中方块
		selected_block = { click_row, click_col };
		board[click_row][click_col].selected = true;
	}
	else {
		if (selected_block.first == click_row && selected_block.second == click_col) {
			// 点击同一方块，取消选中
			board[selected_block.first][selected_block.second].selected = false;
			selected_block = { -1, -1 };
			return;
		}

		if (is_connected(selected_block.first, selected_block.second, click_row, click_col)) {
			// 可连接：消除方块+更新得分和连击
			board[selected_block.first][selected_block.second].eliminated = true;
			board[click_row][click_col].eliminated = true;
			board[selected_block.first][selected_block.second].selected = false;

			combo++;
			score += current_level.base_score + (combo - 1) * current_level.combo_add;
		}
		else {
			// 不可连接：取消选中+重置连击
			board[selected_block.first][selected_block.second].selected = false;
			combo = 0;
		}

		selected_block = { -1, -1 };
	}
}

// 胜利检测：所有方块都已消除则胜利
bool check_win() {
	for (int i = 0; i < ROW; i++) {
		for (int j = 0; j < COL; j++) {
			if (!board[i][j].eliminated) return false;
		}
	}
	return true;
}

// 检查是否还有可连接的方块：避免死局，用于失败检测
bool has_connectable() {
	for (int i = 0; i < ROW; i++) {
		for (int j = 0; j < COL; j++) {
			if (board[i][j].eliminated) continue;
			for (int x = i; x < ROW; x++) {
				for (int y = (x == i) ? j + 1 : 0; y < COL; y++) {
					if (!board[x][y].eliminated &&
						board[i][j].value == board[x][y].value &&
						is_connected(i, j, x, y)) {
						return true;
					}
				}
			}
		}
	}
	return false;
}

// 失败检测：时间用完 或 无可用连接方块
bool check_lose() {
	if (remaining_time <= 0) return true;
	return !has_connectable();
}

// 更新计时：暂停时停止计时，恢复时修正时间戳
void update_timer() {
	if (!is_paused && current_state == STATE_GAME) {
		clock_t now = clock();
		double elapsed = (double)(now - start_time) / CLOCKS_PER_SEC;
		remaining_time = current_level.time_limit - (int)elapsed;
		if (remaining_time < 0) remaining_time = 0;
	}
}

// 提示道具：高亮显示一对可连接的方块（消耗提示次数）
void use_hint() {
	if (current_level.prop_hint <= 0) return;

	// 清除之前的选中状态
	if (selected_block.first != -1) {
		board[selected_block.first][selected_block.second].selected = false;
		selected_block = { -1, -1 };
	}

	// 寻找第一对可连接方块并高亮
	for (int i = 0; i < ROW; i++) {
		for (int j = 0; j < COL; j++) {
			if (board[i][j].eliminated) continue;
			for (int x = i; x < ROW; x++) {
				for (int y = (x == i) ? j + 1 : 0; y < COL; y++) {
					if (!board[x][y].eliminated &&
						board[i][j].value == board[x][y].value &&
						is_connected(i, j, x, y)) {
						board[i][j].selected = true;
						board[x][y].selected = true;
						selected_block = { -1, -1 };
						current_level.prop_hint--;
						return;
					}
				}
			}
		}
	}
}

// 洗牌道具：打乱未消除方块的顺序（消耗洗牌次数）
void use_shuffle() {
	if (current_level.prop_shuffle <= 0) return;

	std::vector<int> values;
	std::vector<std::pair<int, int>> positions;

	// 收集未消除方块的数值和位置
	for (int i = 0; i < ROW; i++) {
		for (int j = 0; j < COL; j++) {
			if (!board[i][j].eliminated) {
				values.push_back(board[i][j].value);
				positions.push_back({ i, j });
			}
		}
	}

	// 洗牌并重新赋值
	manual_shuffle(values);
	for (size_t i = 0; i < positions.size(); i++) {
		int r = positions[i].first;
		int c = positions[i].second;
		board[r][c].value = values[i];
	}

	// 清除选中状态
	if (selected_block.first != -1) {
		board[selected_block.first][selected_block.second].selected = false;
		selected_block = { -1, -1 };
	}
	path.clear();
	current_level.prop_shuffle--;
}

// 处理按钮点击：切换游戏状态（开始/暂停/重新开始/返回首页）
void handle_button_click(int mouse_x, int mouse_y) {
	if (current_state == STATE_START) {
		if (mouse_x >= 280 && mouse_x <= 480 && mouse_y >= 280 && mouse_y <= 330) {
			init_level_config(1);
			init_board();
			start_time = clock();
			current_state = STATE_GAME;
		}
	}
	else if (current_state == STATE_PAUSE) {
		if (mouse_x >= 300 && mouse_x <= 500 && mouse_y >= 250 && mouse_y <= 300) {
			// 继续游戏：修正时间戳，保证计时连续
			is_paused = false;
			current_state = STATE_GAME;
			start_time = clock() - (current_level.time_limit - remaining_time) * CLOCKS_PER_SEC;
		}
		else if (mouse_x >= 300 && mouse_x <= 500 && mouse_y >= 330 && mouse_y <= 380) {
			current_state = STATE_START;
		}
	}
	else if (current_state == STATE_WIN || current_state == STATE_LOSE) {
		if (mouse_x >= 300 && mouse_x <= 500 && mouse_y >= 300 && mouse_y <= 350) {
			// 重新开始：重置关卡和棋盘
			init_level_config(1);
			init_board();
			start_time = clock();
			current_state = STATE_GAME;
		}
		else if (mouse_x >= 300 && mouse_x <= 500 && mouse_y >= 380 && mouse_y <= 430) {
			current_state = STATE_START;
		}
	}
}

// 绘制方块：区分选中/未选中样式，已消除的方块不绘制
void draw_block(Block& block) {
	if (block.eliminated) return;

	// 绘制图片
	if (block.value >= 1 && block.value <= 10) {
		putimage(block.x, block.y, &images[block.value - 1]);
	}

	// 绘制选中边框
	if (block.selected) {
		setlinecolor(RGB(0, 120, 215));      // 蓝色边框
		setlinestyle(PS_SOLID, 3);         // 粗边框
		rectangle(block.x, block.y, block.x + BLOCK_SIZE, block.y + BLOCK_SIZE);
	}
	else {
		setlinecolor(RGB(100, 100, 100));  // 灰色边框
		setlinestyle(PS_SOLID, 1);         // 细边框
		rectangle(block.x, block.y, block.x + BLOCK_SIZE, block.y + BLOCK_SIZE);
	}
}

// 绘制棋盘网格线：辅助视觉定位
void draw_board_grid() {
	setlinecolor(RGB(200, 200, 200));
	for (int i = 0; i <= ROW; i++) {
		line(BOARD_OFFSET_X, BOARD_OFFSET_Y + i * BLOCK_SIZE,
			BOARD_OFFSET_X + COL * BLOCK_SIZE, BOARD_OFFSET_Y + i * BLOCK_SIZE);
	}
	for (int j = 0; j <= COL; j++) {
		line(BOARD_OFFSET_X + j * BLOCK_SIZE, BOARD_OFFSET_Y,
			BOARD_OFFSET_X + j * BLOCK_SIZE, BOARD_OFFSET_Y + ROW * BLOCK_SIZE);
	}
}

// 绘制连线：支持棋盘外路径，拐点处绘制标记（可视化连接路径）
void draw_path() {
	// 函数被禁用，不再绘制红线
	return;
}

// 绘制游戏信息栏：得分、连击、剩余时间、道具数量、时间进度条
void draw_game_info() {
	setfillcolor(RGB(240, 240, 240));
	solidrectangle(10, 10, WINDOW_WIDTH - 10, 70);

	settextstyle(20, 0, "微软雅黑");
	settextcolor(BLACK);
	setbkmode(TRANSPARENT);

	char score_text[50];
	sprintf_s(score_text, "得分: %d  连击: %d", score, combo);
	outtextxy(20, 25, score_text);

	char time_text[50];
	sprintf_s(time_text, "剩余时间: %d 秒", remaining_time);
	outtextxy(250, 25, time_text);

	char prop_text[100];
	sprintf_s(prop_text, "提示(H): %d  洗牌(S): %d  暂停(P)",
		current_level.prop_hint, current_level.prop_shuffle);
	outtextxy(450, 25, prop_text);

	// 时间进度条：红色（<30秒）/绿色（≥30秒）
	if (remaining_time < 30) {
		setfillcolor(RGB(255, 100, 100));
	}
	else {
		setfillcolor(RGB(100, 255, 100));
	}
	int time_bar_width = (WINDOW_WIDTH - 20) * remaining_time / current_level.time_limit;
	solidrectangle(10, 75, 10 + time_bar_width, 85);
}

// 绘制开始页面：游戏标题、玩法说明、开始按钮
void draw_start_page() {
	setfillcolor(WHITE);
	solidrectangle(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

	setbkmode(TRANSPARENT); // 设置文本背景透明

	settextstyle(50, 0, "SimHei"); // 使用更通用的黑体字体
	settextcolor(RGB(0, 120, 215));
	outtextxy(250, 150, "图片连连看");

	settextstyle(20, 0, "SimHei"); // 使用更通用的黑体字体
	settextcolor(RGB(100, 100, 100));
	outtextxy(220, 220, "使用鼠标点击相同图案的方块");
	outtextxy(200, 250, "路径可以绕过棋盘外部连接（最多两个弯）");

	setfillcolor(RGB(76, 175, 80));
	solidrectangle(300, 300, 500, 350);
	settextstyle(30, 0, "SimHei"); // 使用更通用的黑体字体
	settextcolor(WHITE);
	outtextxy(360, 310, "开始游戏");

	setfillcolor(RGB(255, 152, 0));
	solidrectangle(300, 380, 500, 430);
	settextstyle(20, 0, "SimHei"); // 使用更通用的黑体字体
	settextcolor(WHITE);
	outtextxy(320, 395, "按 ESC 返回首页");
	outtextxy(320, 415, "按 P 暂停游戏");
}

// 绘制暂停页面：暂停提示、继续/返回按钮
void draw_pause_page() {
	setfillcolor(RGB(100, 100, 100));
	solidrectangle(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
	setbkmode(TRANSPARENT);

	settextstyle(50, 0, "微软雅黑");
	settextcolor(WHITE);
	outtextxy(300, 150, "游戏暂停");

	setfillcolor(RGB(76, 175, 80));
	solidrectangle(300, 250, 500, 300);
	settextstyle(25, 0, "微软雅黑");
	outtextxy(350, 255, "继续游戏");

	setfillcolor(RGB(244, 67, 54));
	solidrectangle(300, 330, 500, 380);
	outtextxy(370, 335, "返回首页");

	settextstyle(20, 0, "微软雅黑");
	settextcolor(WHITE);
	outtextxy(280, 420, "按 ESC 也可以返回首页");
}

// 绘制胜利页面：胜利提示、得分、重新开始/返回按钮
void draw_win_page() {
	setfillcolor(WHITE);
	solidrectangle(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

	settextstyle(50, 0, "微软雅黑");
	settextcolor(RGB(76, 175, 80));
	outtextxy(250, 120, "恭喜胜利！");

	settextstyle(30, 0, "微软雅黑");
	settextcolor(BLACK);
	char score_text[50];
	sprintf_s(score_text, "最终得分: %d", score);
	outtextxy(300, 200, score_text);

	char combo_text[50];
	sprintf_s(combo_text, "最高连击: %d", combo);
	outtextxy(300, 250, combo_text);

	setfillcolor(RGB(76, 175, 80));
	solidrectangle(250, 320, 450, 370);
	settextstyle(25, 0, "微软雅黑");
	settextcolor(WHITE);
	outtextxy(280, 325, "重新开始");

	setfillcolor(RGB(244, 67, 54));
	solidrectangle(250, 400, 450, 450);
	outtextxy(320, 405, "返回首页");
}

// 绘制失败页面：失败提示、得分、失败原因、重新开始/返回按钮
void draw_lose_page() {
	setfillcolor(WHITE);
	solidrectangle(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

	settextstyle(50, 0, "微软雅黑");
	settextcolor(RGB(244, 67, 54));
	outtextxy(250, 120, "游戏失败！");

	settextstyle(30, 0, "微软雅黑");
	settextcolor(BLACK);
	char score_text[50];
	sprintf_s(score_text, "最终得分: %d", score);
	outtextxy(300, 200, score_text);

	if (remaining_time <= 0) {
		outtextxy(280, 250, "时间用完了！");
	}
	else {
		outtextxy(250, 250, "没有可连接的方块了！");
	}

	setfillcolor(RGB(76, 175, 80));
	solidrectangle(250, 320, 450, 370);
	settextstyle(25, 0, "微软雅黑");
	settextcolor(WHITE);
	outtextxy(280, 325, "重新开始");

	setfillcolor(RGB(244, 67, 54));
	solidrectangle(250, 400, 450, 450);
	outtextxy(320, 405, "返回首页");
}

// 绘制游戏状态：根据当前状态绘制对应界面
void draw_game_state() {
	cleardevice();

	switch (current_state) {
	case STATE_START:
		draw_start_page();
		break;
	case STATE_GAME:
		draw_board_grid();
		for (int i = 0; i < ROW; i++) {
			for (int j = 0; j < COL; j++) {
				draw_block(board[i][j]);
			}
		}
		draw_path();
		draw_game_info();
		break;
	case STATE_PAUSE:
		draw_pause_page();
		break;
	case STATE_WIN:
		draw_win_page();
		break;
	case STATE_LOSE:
		draw_lose_page();
		break;
	}
}

// 主函数：游戏入口，消息循环（鼠标/键盘/窗口事件）+ 状态更新 + 绘制
int main() {
	initgraph(WINDOW_WIDTH, WINDOW_HEIGHT);
	SetWindowText(GetHWnd(), "连连看游戏（支持外部连接）");
	setbkcolor(WHITE);

	srand((unsigned int)time(NULL));

	// 加载图片资源
	loadimage(&images[0], _T("images/1.png"), BLOCK_SIZE, BLOCK_SIZE);
	loadimage(&images[1], _T("images/2.png"), BLOCK_SIZE, BLOCK_SIZE);
	loadimage(&images[2], _T("images/3.png"), BLOCK_SIZE, BLOCK_SIZE);
	loadimage(&images[3], _T("images/4.png"), BLOCK_SIZE, BLOCK_SIZE);
	loadimage(&images[4], _T("images/5.png"), BLOCK_SIZE, BLOCK_SIZE);
	loadimage(&images[5], _T("images/6.png"), BLOCK_SIZE, BLOCK_SIZE);
	loadimage(&images[6], _T("images/7.png"), BLOCK_SIZE, BLOCK_SIZE);
	loadimage(&images[7], _T("images/8.png"), BLOCK_SIZE, BLOCK_SIZE);
	loadimage(&images[8], _T("images/9.png"), BLOCK_SIZE, BLOCK_SIZE);
	loadimage(&images[9], _T("images/10.png"), BLOCK_SIZE, BLOCK_SIZE);

	init_level_config(1);
	init_board();
	start_time = clock();

	ExMessage msg;
	while (true) {
		BeginBatchDraw(); // 批量绘制：减少闪烁，提升流畅度

		// 消息处理：鼠标点击、键盘按键、窗口关闭
		while (peekmessage(&msg)) {
			switch (msg.message) {
			case WM_LBUTTONDOWN:
				if (current_state == STATE_GAME) {
					handle_block_click(msg.x, msg.y); // 游戏中：处理方块点击
				}
				else {
					handle_button_click(msg.x, msg.y); // 非游戏中：处理按钮点击
				}
				break;
			case WM_KEYDOWN:
				if (current_state == STATE_GAME) {
					switch (msg.vkcode) {
					case 'H': case 'h': use_hint(); break;    // 提示道具
					case 'S': case 's': use_shuffle(); break; // 洗牌道具
					case 'P': case 'p': is_paused = true; current_state = STATE_PAUSE; break; // 暂停
					case VK_ESCAPE: current_state = STATE_START; break; // 返回首页
					}
				}
				else if (current_state == STATE_PAUSE) {
					if (msg.vkcode == VK_ESCAPE) {
						current_state = STATE_START; // 暂停时ESC返回首页
					}
				}
				break;
			case WM_CLOSE:
				closegraph();
				return 0; // 关闭窗口退出游戏
			}
		}

		// 游戏状态更新：计时、胜利/失败检测
		if (current_state == STATE_GAME && !is_paused) {
			update_timer();
			if (check_win()) {
				current_state = STATE_WIN;
			}
			else if (check_lose()) {
				current_state = STATE_LOSE;
			}
		}

		// 绘制界面
		draw_game_state();

		EndBatchDraw();
		Sleep(16); // 控制帧率≈60帧/秒，降低CPU占用
	}

	closegraph();
	return 0;
}