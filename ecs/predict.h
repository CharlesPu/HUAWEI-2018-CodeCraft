#ifndef __ROUTE_H__
#define __ROUTE_H__

#include "lib_io.h"
#include <unordered_map>
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <iostream>
#include "Server.h"

using namespace std;
#define FLAVOR_MAX_NUM 50
#define CPU 0 
#define MEM 1

// #define DENOISING

// #define INCREASING_WEIGHT  //递增权重法
#define SPECIAL_PARA		//蜜汁参数1.26法
// #define MINE 	//自己发明的方法
// #define LAST_3_WEEKS //后三周拟合法

// #define FIRST_FIT
// #define BEST_FIT
#define FRIE_FIT

typedef struct _machine  //物理机和虚拟机共有的规格参数
{
	int CPU_num;
	int MEM_size;
	int DISK_size;
}MACHINE;

typedef struct _flavor	//需要预测的虚拟机的参数
{
	int flavor_id; //从1开始
	MACHINE* flavor_para;	//虚拟机参数
	int predict_num;	//存放最后预测的数量
	unsigned int* num_per_day;	//一个数组，存放训练集中每一天此flavor的总数
	int* week_day;
}FLAVOR;

typedef struct _host
{
	MACHINE host_para_left; //每个物理机剩余的资源
	unsigned int flavors_arr[FLAVOR_MAX_NUM]; //每个物理机放置的虚拟机个数，此数组下标表示flavorid，值表示此flavor个数
}HOST;

//录入15种虚拟机的规格参数
const MACHINE flavors_para[FLAVOR_MAX_NUM]={
	{1, 1024},
	{1, 2048},
	{1, 4096},
	{2, 2048},
	{2, 4096},
	{2, 8192},
	{4, 4096},
	{4, 8192},
	{4, 16384},
	{8, 8192},
	{8, 16384},
	{8, 32768},
	{16, 16384},
	{16, 32768},
	{16, 65536},

	//待录入、、、

};

void predict_server(char * info[MAX_INFO_NUM], char * data[MAX_DATA_NUM], int info_num, int data_num, char * filename);
void PreProcess(char * data[MAX_DATA_NUM], int data_num, char * info[MAX_INFO_NUM], int info_num, FLAVOR * predict_flavors);//训练集数据预处理，返回最终要预测的flavor数组
void PredictNum(FLAVOR* predict_flavors, int predict_days);//预测虚拟机在预期天数里的个数，改变了传入参数flavor数组元素的predict_num成员变量
int ArrangeFlavors(int dimension, FLAVOR* predict_flavors, HOST* hosts);//根据之前预测的每个虚拟机的个数进行布置，产生输出文件
void WriteResults(FLAVOR* predict_flavors, int host_num, HOST* hosts, char * filename);

int GetFlavorId( char* record, const char split);
int CountDate(char* record, const int* ref);
void GetDate(char* record, int ret[3]);
int DaysDiff(int year_start, int month_start, int day_start, int year_end, int month_end, int day_end);
int StringSplitAndAtoi(const char* str, const char* split, int num[3]);
int GetWeekDay(int y, int m, int d);
void BubbleSort(FLAVOR fla[], int size, int dimension);
void GradientDescent(char* train_data[3], int train_num, char* result_data, char theta[3]);

vector<Server> put_flavors_to_servers(unordered_map<string, int> map_predict_num_flavors, 
	unordered_map<string, Flavor> map_flavor_cpu_mem,int server_mem, int server_cpu, bool CPUorMEM);
void myWrite(vector<Server> serv, FLAVOR* predict_flavors, char * filename);

#endif
