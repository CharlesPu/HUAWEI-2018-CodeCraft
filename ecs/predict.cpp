#include "predict.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Server.h"
#include <math.h>
//全局变量
MACHINE physical_machine;
int dimension = 0; //维度值 CPU or MEMs
int predict_type_num = 0;	//要预测的虚拟机种类数
int predict_flavors_sum = 0; //要预测的虚拟机总数量数
int predict_days = 0; //要预测的天数，7~14 
int days_total = 0; //训练集总共的天数

void predict_server(char * info[MAX_INFO_NUM], char * data[MAX_DATA_NUM], int info_num, int data_num, char * filename)
{
	//首先找到要预测的flavor种类数
	predict_type_num = atoi(info[2]);
	FLAVOR predict_flavors[predict_type_num];
	PreProcess(data, data_num, info, info_num, predict_flavors);

	PredictNum(predict_flavors, predict_days);
#ifdef FRIE_FIT
	unordered_map<string, int> map_predict_num_flavors;
	unordered_map<string, Flavor> map_flavor_cpu_mem;
	for (int i = 0; i < predict_type_num; ++i)
	{
		char* tem = (char*)malloc(10);
		sprintf(tem, "flavor%d", predict_flavors[i].flavor_id);
		map_predict_num_flavors[tem] = predict_flavors[i].predict_num;
		map_flavor_cpu_mem[tem] = Flavor(tem, predict_flavors[i].flavor_para->MEM_size, predict_flavors[i].flavor_para->CPU_num);
		free(tem);
	}
	vector<Server> servers = put_flavors_to_servers(map_predict_num_flavors, map_flavor_cpu_mem, physical_machine.MEM_size, physical_machine.CPU_num, dimension);
	myWrite(servers, predict_flavors, filename);
#else
	HOST hosts_arr[predict_flavors_sum]; //可用物理机数组
	int host_sum = ArrangeFlavors(dimension, predict_flavors, hosts_arr);
	WriteResults(predict_flavors, host_sum, hosts_arr, filename);
#endif
}
//训练集数据预处理，返回最终要预测的flavor数组
void PreProcess(char * data[MAX_DATA_NUM], int data_num, char * info[MAX_INFO_NUM], int info_num, FLAVOR * predict_flavors)
{
	//大数组
	FLAVOR all_flavors[FLAVOR_MAX_NUM];
	//得到物理机参数
	int para[3];
	StringSplitAndAtoi(info[0], " ", para);
	physical_machine.CPU_num = para[0];//printf("%d\n",physical_machine.CPU_num);
	physical_machine.MEM_size = para[1] * 1024;//printf("%d\n",physical_machine.MEM_size);
	physical_machine.DISK_size = para[2];//printf("%d\n",physical_machine.DISK_size);
	//得到参照维度
	char* dim = (char*)malloc(3);
	memcpy(dim, info[4 + predict_type_num], 3);
	if( !strncmp(dim, "CPU", 3) ) dimension = CPU;
		else if( !strncmp(dim, "MEM", 3) )dimension = MEM;//printf("dimensin:%d\n", dimension);
	free(dim);
	//找到训练集开始的时间
    int y_m_d_ref[3];
    GetDate(data[0], y_m_d_ref);
    //找到需要预测的时间段的天数
    predict_days = CountDate(info[7 + predict_type_num], y_m_d_ref) - CountDate(info[6 + predict_type_num], y_m_d_ref);//printf("%d\n", predict_days);
	//找到训练集最大的天数
	int count = data_num - 1;
	while (!strncmp(data[count], "\r\n", 2) || !strncmp(data[count], "\n", 1)) count--;//printf("1\n");
	days_total = CountDate(data[count], y_m_d_ref) + 1;//printf("days_total:%d\n",  days_total);
	//回过头来初始化下每种虚拟机每天的数量数组
	for(int i = 0; i < FLAVOR_MAX_NUM; i++)
	{
		all_flavors[i].flavor_id = i + 1;
		all_flavors[i].num_per_day = (unsigned int *)malloc(days_total * sizeof(unsigned int));
		all_flavors[i].week_day = (int *)malloc(days_total * sizeof(int));
		memset(all_flavors[i].num_per_day, 0, days_total * sizeof(unsigned int));
		memset(all_flavors[i].week_day, -1, days_total * sizeof(int));
		all_flavors[i].flavor_para = (MACHINE* )&flavors_para[i];
	}
	//构造大数组！
	for (int i = 0; i < data_num; ++i)
	{
		if (strncmp(data[i], "\r\n", 2) && strncmp(data[i], "\n", 1))
		{	
			int fla_id = GetFlavorId(data[i], '\t');
			int day_diff = CountDate(data[i], y_m_d_ref);
			all_flavors[fla_id - 1].num_per_day[day_diff]++;
		}
	}
	//生成返回数组，即要预测的flavor
	for(int i = 0; i < predict_type_num; i++)
	{
		int fla_id = GetFlavorId(info[3+i], ' '); //printf("%d", fla_id);
		predict_flavors[i] = all_flavors[fla_id - 1];
	}
	int w_day_ref = GetWeekDay(y_m_d_ref[0], y_m_d_ref[1], y_m_d_ref[2]);
	for(int i = 0; i < predict_type_num; i++)
	{
		for (int j = 0; j < days_total; ++j)
		{
			all_flavors[i].week_day[j] = (w_day_ref + j) % 7;
		}
	}
#ifdef DENOISING
	for (int i = 0; i < predict_type_num; ++i)
	{
		for (int j = 0; j < days_total; j += 7)
		{
			float u = 0, thegma = 0, sum = 0;
			int k = 0;
			for (k = 0; k < 7 && (j + k) < days_total; ++k)
				sum += predict_flavors[i].num_per_day[j + k];
			u = sum / k;
			sum = 0;
			for (k = 0; k < 7 && (j + k) < days_total; ++k)
				sum += (predict_flavors[i].num_per_day[j + k] - u) * (predict_flavors[i].num_per_day[j + k] - u);
			thegma = sqrt(sum / k);
			for (k = 0; k < 7 && (j + k) < days_total; ++k)
			{
				predict_flavors[i].num_per_day[j + k] = 
					(predict_flavors[i].num_per_day[j + k] > (u + 3 * thegma)) ? (u + 3 * thegma) : predict_flavors[i].num_per_day[j + k];
			}
			for (k = 0; k < 7 && (j + k) < days_total; ++k)
			{
				if (predict_flavors[i].week_day[j + k] != 0 && predict_flavors[i].num_per_day[j + k] == 0)
				{
					predict_flavors[i].num_per_day[j + k] = u;
				}
			}
		}
	}
#endif
	// for (int i = 0; i < predict_type_num; ++i)
	// {
	// 	printf("flavor%d: ", predict_flavors[i].flavor_id);
	// 	for (int j = 0; j < days_total ; ++j)
	// 	{
	// 		// printf("%d ", predict_flavors[i].num_per_day[j]);
	// 		printf("%d ", predict_flavors[i].week_day[j]);
	// 	}
	// 	printf("\n");
	// }
}
//预测虚拟机在预期天数里的个数，改变了传入参数flavor数组元素的predict_num成员变量
void PredictNum(FLAVOR* predict_flavors, int predict_days)	
{
	//递增权重法
#ifdef INCREASING_WEIGHT
	//初始化好一个数组
	float cnt[predict_type_num][predict_days];
	for(int i = 0;i < predict_type_num; i++)
		for(int j = 0; j < predict_days; j++)
			cnt[i][j] = 0;
	//预先计算好权重
	float weight[predict_days][days_total + predict_days];
	for (int i = 0; i < predict_days; ++i)
	{
		float bas = (days_total + i + 1) * (days_total + i) * 10000;//求权重的基数大小，在变
		for (int j = 0; j < days_total + i; ++j)
		{
			weight[i][j] = (j + 1) * 10000 * 2  / (float)bas;
		}
	}
	//开始预测每一天
	for (int i = 0; i < predict_type_num; i++)
	{
		for (int j = 0; j < predict_days; j++)
		{
			for (int k = 0; k < days_total + j; k++)
				if(k < days_total) 
					cnt[i][j] += (predict_flavors[i].num_per_day[k] * weight[j][k] );
				else
					cnt[i][j] += ( cnt[i][k - days_total] * weight[j][k] );
			// printf("%.5f ", cnt[i][j] );
		}
	}
	for (int i = 0; i < predict_type_num; ++i)
	{
		float sum = 0;
		for (int j = 0; j < predict_days; ++j)
		{
			sum += cnt[i][j];
		}
		predict_flavors[i].predict_num = sum;
		predict_flavors_sum += predict_flavors[i].predict_num;
		// printf("flavor%d sum:%d\n", predict_flavors[i].flavor_id, predict_flavors[i].predict_num);
	}
	//迷之调参法.....
#endif
#ifdef SPECIAL_PARA
	for (int i = 0; i < predict_type_num; ++i)
	{
		float sum = 0;
		for (int j = days_total - predict_days; j < days_total; ++j)
		{
			sum += predict_flavors[i].num_per_day[j];
		}
		predict_flavors[i].predict_num = sum * 1.26;
		predict_flavors_sum += predict_flavors[i].predict_num;
		// printf("flavor%d sum:%d\n", predict_flavors[i].flavor_id, predict_flavors[i].predict_num);	
	}
#endif 
#ifdef MINE
//初始化好一个数组
	float cnt[predict_type_num][predict_days];
	for(int i = 0;i < predict_type_num; i++)
		for(int j = 0; j < predict_days; j++)
			cnt[i][j] = 0;
	//预先计算好权重
	float weight[predict_days][days_total + predict_days - predict_days];
	for (int i = 0; i < predict_days; ++i)
	{
		float bas = (days_total - predict_days + i + 1) * (days_total - predict_days + i) * 10000;//求权重的基数大小，在变
		for (int j = 0; j < days_total - predict_days + i; ++j)
		{
			weight[i][j] = (j + 1) * 10000 * 2  / (float)bas;
		}
	}
	//开始预测每一天
	for (int i = 0; i < predict_type_num; i++)
	{
		for (int j = 0; j < predict_days; j++)
		{
			for (int k = 0; k < days_total - predict_days  + j; k++)
				if(k < days_total - predict_days ) 
					cnt[i][j] += (predict_flavors[i].num_per_day[k] * weight[j][k] );
				else
					cnt[i][j] += ( cnt[i][k - days_total - predict_days ] * weight[j][k] );
		}
	}
	for (int i = 0; i < predict_type_num; ++i)
	{
		float sum = 0;
		for (int j = 0; j < predict_days; ++j)
		{
			sum += cnt[i][j];
		}
		for (int j = days_total - predict_days; j < days_total; ++j)
		{
			sum += predict_flavors[i].num_per_day[j] * 0.5;
		}
		predict_flavors[i].predict_num = sum * 1.7;
		predict_flavors_sum += predict_flavors[i].predict_num;
		// printf("flavor%d sum:%d\n", predict_flavors[i].flavor_id, predict_flavors[i].predict_num);
	}
#endif
#ifdef LAST_3_WEEKS
	float arr[predict_type_num][3];
	for (int i = 0; i < predict_type_num; ++i)
	{
		for (int j = 0; j < 3; j++)
		{
			float sum = 0;
			for (int k = 0; k < 7; ++k)
			{
				sum += predict_flavors[i].num_per_day[days_total - 1 - k - 7 * j];
			}
			arr[i][2 - j] = sum / 7;
			// printf("[%d][%d]:%f\n", i, 2 - j, arr[i][2 - j]);
		}
	}
	
	for (int i = 0; i < predict_type_num; ++i)
	{
		float k = 0.0, b = 0.0;
		float Xsum=0.0;
		float X2sum=0.0;
		float Ysum=0.0;
		float XY=0.0;
		for (int j = 0; j < 3; ++j)
		{
			Xsum += (j + 1);
			Ysum += arr[i][j];
			XY += (j + 1) * arr[i][j];
			X2sum += (j + 1) * (j + 1);
		}
		k = ((Xsum * Ysum) / 3 - XY) / ((Xsum * Xsum) / 3 - X2sum);
		b = (Ysum - k * Xsum) / 3;
		// printf("i:%d----k:%lf,b:%lf\n", i, k, b);
		predict_flavors[i].predict_num = (4 * k + b) * predict_days;
		// printf("[%d]:%d\n", i, predict_flavors[i].predict_num);
		predict_flavors_sum += predict_flavors[i].predict_num;
	}
	
#endif
}
//根据之前预测的每个虚拟机的个数进行布置，产生输出文件
int ArrangeFlavors(int dimension, FLAVOR* predict_flavors, HOST* hosts)
{
	FLAVOR flavors_tem[predict_flavors_sum]; //待放置的虚拟机数组
	//初始化物理机数组
	for (int i = 0; i < predict_flavors_sum; ++i)
	{
		hosts[i].host_para_left.CPU_num = physical_machine.CPU_num;
		hosts[i].host_para_left.MEM_size = physical_machine.MEM_size;
		hosts[i].host_para_left.DISK_size = physical_machine.DISK_size;
		memset(hosts[i].flavors_arr, 0, FLAVOR_MAX_NUM * sizeof(unsigned int));
	}
	//初始化临时flavor数组
	for (int i = 0, j = 0; j < predict_type_num; ++j)
	{
		for (int k = 0; k < predict_flavors[j].predict_num; ++k)
		{
			flavors_tem[i++] = predict_flavors[j];
		}
	}
	// for (int i = 0; i < predict_flavors_sum; ++i)
	// 	printf("%d ", flavors_tem[i].flavor_para->MEM_size);
	//按照维度排好序
	BubbleSort(flavors_tem, predict_flavors_sum, dimension);

	int hosts_sum = 0;
	// first-fit
#ifdef FIRST_FIT
	for (int i = 0; i < predict_flavors_sum; ++i)
	{
		for (int j = 0; j < predict_flavors_sum; ++j)//对于每个物理机
		{
			if (hosts[j].host_para_left.CPU_num >= flavors_tem[i].flavor_para->CPU_num
				&& hosts[j].host_para_left.MEM_size >= flavors_tem[i].flavor_para->MEM_size)
			{
				if (hosts[j].host_para_left.CPU_num == physical_machine.CPU_num)//如果是新的
					hosts_sum++;
				hosts[j].host_para_left.CPU_num -= flavors_tem[i].flavor_para->CPU_num;
				hosts[j].host_para_left.MEM_size -= flavors_tem[i].flavor_para->MEM_size;
				hosts[j].flavors_arr[flavors_tem[i].flavor_id - 1]++;
				break;
			}
		}
	}
#endif
#ifdef BEST_FIT
	// best-fit
	for (int i = 0; i < predict_flavors_sum; ++i)
	{
		int id = 0, para_left = *((int *)(&physical_machine) + dimension);
		for (int j = 0; j < hosts_sum; ++j)
		{
			if (hosts[j].host_para_left.CPU_num >= flavors_tem[i].flavor_para->CPU_num
				&& hosts[j].host_para_left.MEM_size >= flavors_tem[i].flavor_para->MEM_size)
				if( *((int *)&(hosts[j].host_para_left) + dimension) < para_left)
				{
					id = j;
					para_left = *((int *)&(hosts[j].host_para_left) + dimension);
				}
		}
		if( para_left != *((int *)(&physical_machine) + dimension))//如果在已放置的物理机中找到可以放置的地方
		{
			hosts[id].host_para_left.CPU_num -= flavors_tem[i].flavor_para->CPU_num;
			hosts[id].host_para_left.MEM_size -= flavors_tem[i].flavor_para->MEM_size;
			hosts[id].flavors_arr[flavors_tem[i].flavor_id - 1]++;
		}else //如果在已布置的物理机中没找到可以放置的地方，那就新开一个物理机
		{					
			hosts[hosts_sum].host_para_left.CPU_num -= flavors_tem[i].flavor_para->CPU_num;
			hosts[hosts_sum].host_para_left.MEM_size -= flavors_tem[i].flavor_para->MEM_size;
			hosts[hosts_sum].flavors_arr[flavors_tem[i].flavor_id - 1]++;
			hosts_sum++;
		}											
	}
#endif	
	// printf("host_sum:%d\n", hosts_sum);
	// for (int i = 0; i < hosts_sum; ++i)
	// {
	// 	printf("host %d: ", i);
	// 	for (int j = 0; j < predict_type_num; ++j)
	// 	{
	// 		printf("%d ", hosts[i].flavors_arr[j]);
	// 	}
	// 	printf("\n");
	// }
	return hosts_sum;
}

void WriteResults(FLAVOR* predict_flavors, int host_num, HOST* hosts, char * filename)
{
	char result_file[200];
	// int posi = Myitoa(predict_flavors_sum, result_file, 10);
	// posi += sprintf(result_file + posi, "\n\n");
	int posi = sprintf(result_file, "%d\n", predict_flavors_sum);

	for (int i = 0; i < predict_type_num; ++i)
	{
		posi += sprintf(result_file + posi, "flavor%d ", predict_flavors[i].flavor_id);
		posi += sprintf(result_file + posi, "%d\n", predict_flavors[i].predict_num);
	}
	posi += sprintf(result_file + posi, "\n");
	posi += sprintf(result_file + posi, "%d", host_num);

	for (int i = 0; i < host_num; ++i)
	{
		posi += sprintf(result_file + posi, "\n%d", i + 1);
		for (int j = 0; j < FLAVOR_MAX_NUM; ++j)
		{
			if (hosts[i].flavors_arr[j] != 0)
			{
				posi += sprintf(result_file + posi, " flavor");
				posi += sprintf(result_file + posi, "%d ", j + 1);
				posi += sprintf(result_file + posi, "%d", hosts[i].flavors_arr[j]);
			}
		}
	}
	// printf("%s\n", result_file);
	write_result(result_file, filename);
}

int GetFlavorId( char* record, const char split)
{
	char* p = strstr((char*)record, (const char *)"flavor");
	char* tem = (char*)malloc(3);
	char* head = tem;
	while(*(p+6) != split)
	{
		*tem = *(p + 6);
		p++;
		tem++;
	}
	sprintf(tem, "\0");
	int fla_id = atoi(head);

	free(head);
	tem = NULL,head = NULL;
	return fla_id;
}
//计算出这一天相对于第一天的序号
int CountDate(char* record, const int* ref)
{	
	int y_m_d[3];
	GetDate(record, y_m_d);
	
    return DaysDiff(ref[0], ref[1], ref[2], y_m_d[0], y_m_d[1], y_m_d[2]);
}
//得到这一天的整型日期值
void GetDate(char* record, int ret[3])
{
	//找到日期字符串
	char* p = strstr((char*)record, (const char *)"flavor");
	char* date = (char*)malloc(11);
	if(p == NULL) //如果是input文件
	{
		memcpy(date, record, 10);
	}else //如果是训练集
	{
		p = strstr(p, (const char *)"\t") + 1;
		memcpy(date, p, 10);
		sprintf(date + 10, "\0");//貌似一定要手动加上字符串结束符!!!!!!
	}
	//分割好，转化为整形存入日期数组中
	StringSplitAndAtoi(date, "-", ret);

    free(date);	
}
int StringSplitAndAtoi(const char* str, const char* split, int num[3])
{
	int i = 0;
	char* ptr = strtok((char*)str, (const char *)split);//第一次调用strtok
    while(ptr != NULL )//当返回值不为NULL时，继续循环
    {
        num[i++] = atoi(ptr); //printf("%d\n",num[i-1]);
        ptr = strtok(NULL, (const char *)split);//继续调用strtok，分解剩下的字符串
    }
    return i-1;
}
int GetWeekDay(int y, int m, int d)
{
	if(3 > m)
	{
		m += 12;
		y--;
	}
	int w=((d+2*m+3*(m+1)/5+y+y/4-y/100+y/400)%7+1)%7;
	return w;
}
int DaysDiff(int year_start, int month_start, int day_start
   			, int year_end, int month_end, int day_end)
{
	int y2, m2, d2;
	int y1, m1, d1;

	m1 = (month_start + 9) % 12;
	y1 = year_start - m1/10;
	d1 = 365*y1 + y1/4 - y1/100 + y1/400 + (m1*306 + 5)/10 + (day_start - 1);
	m2 = (month_end + 9) % 12;
	y2 = year_end - m2/10;
	d2 = 365*y2 + y2/4 - y2/100 + y2/400 + (m2*306 + 5)/10 + (day_end - 1);
	return (d2 - d1);
}

void BubbleSort(FLAVOR fla[], int size, int dimension)
{
    int low = 0, high = size-1 ; 
    // printf("%d\n", *dimension);
    FLAVOR tmp;    
    while (low < high) 
	{    
	    for (int j = low; j < high; ++j) //正向冒泡,找到最大者    
	        if ( *((int *)fla[j].flavor_para + dimension) > *((int *)fla[j+1].flavor_para + dimension))    
	            {tmp = fla[j]; fla[j] = fla[j+1]; fla[j+1] = tmp;}         
	    --high;                 //修改high值, 前移一位    
	    for (int j = high; j > low; --j) //反向冒泡,找到最小者    
	        if ( *((int *)fla[j].flavor_para + dimension) < *((int *)fla[j-1].flavor_para + dimension))     
	            {tmp = fla[j]; fla[j] = fla[j-1]; fla[j-1] = tmp;}    
	    ++low;                  //修改low值,后移一位    
	} 
	// for(int j = 0; j < size ;j++)
	// 	printf("[%d]:%d ", j, *((int *)fla[j].flavor_para + dimension));
	// printf("\n"); 
}

//梯度下降法
void GradientDescent(char* train_data[3], int train_num, char* result_data, char theta[3])
{
	// double matrix[4][3]={{1,4,1},{2,5,1},{5,1,1},{4,2,1}};
    // double result[4]={19,26,19,20};
    double error_sum = 0;
    // double theta[3] = {0,0,0};  //theta的初值全部设为，从零开始
    int i,temp;
    for(temp=1;temp<=1000;temp++)   //这里我没有用之前别人博客上面的参考，他的代码实际上不是run了一千遍，而是run了1000/len(data)遍。
	{                               //temp用来表示迭代次数，然后i表示样本的数量，每次更新temp，都要迭代一轮i
	    int k,j;
	    for(i = 0;i < train_num;i++)          //因为训练数据是4个 所以i从0到3
	    {
	        double h = 0;
	        for(k = 0;k <3;k++)     //k负责迭代的是x的维数，也就是theta的维数，之前说过了，二位数据要设置三维的维度
	        {
	            h = h + theta[k]*train_data[i][k];     //用累加和求预测函数h（x）并且记住，每次求完h算完残差之后，都要把h重新归零
	        }
	        error_sum = h - result_data[i];
	        for(k = 0;k<3;k++)
	        {
	            theta[k] = theta[k] - 0.04*error_sum*train_data[i][k]; //梯度下降的精髓，更新k个theta值，并且减去导数方向乘以x乘以学习率alpha
	        }
	    }
	}
	printf("%lf %lf %lf ",theta[0],theta[1],theta[2]);
}
#ifdef FRIE_FIT
///使用模拟退火算法找最佳的虚拟机放置方式
///输入参数：
///map_predict_num_flavors：上一步预测出来的各种虚拟机数量，key是虚拟机名称，value是虚拟机数量
///map_flavor_cpu_mem：程序输入的虚拟机类型数据，key为虚拟机名称，value是虚拟机类型（包括name，cpu，mem字段）
///server_mem && server_cpu：程序输入的服务器参数，CPU和内存大小
///CPUorMEM：是使CPU利用率最高还是使内存利用率最高
///输出参数：
///res_servers：存放有计算出的最优服务器中虚拟机存放方式，可通过成员flavors访问每个服务器中存放的虚拟机
vector<Server> put_flavors_to_servers(unordered_map<string, int> map_predict_num_flavors, 
	unordered_map<string, Flavor> map_flavor_cpu_mem,int server_mem, int server_cpu, bool CPUorMEM) {

	vector<Flavor> vec_flavors;    //vector用于存放所有预测出来的flavor
	for (auto element : map_predict_num_flavors) {
		//将预测出来的所有虚拟机都加入到vec_flavors中
		while (element.second-- != 0) {
			vec_flavors.push_back(map_flavor_cpu_mem[element.first]);
		}
	}

	//=========================================================================
	//模拟退火算法找最优解
	double min_server = vec_flavors.size() + 1;
	vector<Server> res_servers;  //用于存放最好结果（服务器使用数量最少）
	double T = 100.0;  //模拟退火初始温度
	double Tmin = 1;   //模拟退火终止温度
	double r = 0.9999; //温度下降系数
	vector<int> dice;  //骰子，每次随机投掷，取vector前两个变量作为每次退火需要交换顺序的虚拟机
	for (int i = 0; i < vec_flavors.size(); i++) {
		dice.push_back(i);
	}
	while (T > Tmin) {
		//投掷骰子，如vector前两个数为3和9，则把vec_flavors[3]和vec_flavors[9]进行交换作为新的flavors顺序
		std::random_shuffle(dice.begin(), dice.end());
		auto new_vec_flavors = vec_flavors;

		// if( !new_vec_flavors[dice[0]].name.compare(new_vec_flavors[dice[1]].name)) continue;
		
		std::swap(new_vec_flavors[dice[0]], new_vec_flavors[dice[1]]);

		//把上一步计算出来的虚拟机尝试加入到服务器中

		//先使用一个服务器用于放置虚拟机
		vector<Server> servers;
		Server firstserver(server_mem, server_cpu);
		servers.push_back(firstserver);  
		
		//放置虚拟机主要逻辑
		//如果当前所有服务器都放不下虚拟机，就新建一个服务器用于存放
		for (auto element : new_vec_flavors) {
			auto iter = servers.begin();
			for (; iter != servers.end(); ++iter) {
				if (iter->put_flavor(element)) {
					break;
				}
			}
			if (iter == servers.end()) {
				Server newserver(server_mem, server_cpu);
				newserver.put_flavor(element);
				servers.push_back(newserver);
			}
		}

		//计算本次放置虚拟机耗费服务器评价分数(double型)
		//如果使用了N个服务器，则前N-1个服务器贡献分数为1，第N个服务器分数为资源利用率
		//模拟退火就是得到取得分数最小的放置方式
		double server_num;
		//对于题目关心CPU还是MEM，需要分开讨论，资源利用率计算方法不同
		if (CPUorMEM == CPU)
			server_num = servers.size() - 1 + servers.rbegin()->get_cpu_usage_rate();
		else
			server_num = servers.size() - 1 + servers.rbegin()->get_mem_usage_rate();
		//如果分数更低，则保存结果
		if (server_num < min_server) {
			min_server = server_num;
			res_servers = servers;
			vec_flavors = new_vec_flavors;
		}
		//如果分数更高，则以一定概率保存结果，防止优化陷入局部最优解
		else {
			if (exp((min_server - server_num) / T) > rand() / RAND_MAX) {
				min_server = server_num;
				res_servers = servers;
				vec_flavors = new_vec_flavors;
			}
		}
		T = r * T;  //一次循环结束，温度降低
	}
	return res_servers;
}

void myWrite(vector<Server> serv, FLAVOR* predict_flavors, char * filename)
{
	int server_count = 0;
	for (auto element : serv) {
		server_count++;
	}
	

	char result_file[200];
	int posi = sprintf(result_file, "%d\n", predict_flavors_sum);

	for (int i = 0; i < predict_type_num; ++i)
	{
		posi += sprintf(result_file + posi, "flavor%d ", predict_flavors[i].flavor_id);
		posi += sprintf(result_file + posi, "%d\n", predict_flavors[i].predict_num);
	}
	posi += sprintf(result_file + posi, "\n");
	posi += sprintf(result_file + posi, "%d", server_count);

	int i = 0;
	for (auto element : serv) {
		unordered_map<string, int> map_flavor_num;
		posi += sprintf(result_file + posi, "\n%d", i + 1);

		for (auto flavor : element.flavors) {

			map_flavor_num[flavor.name]++;
		}
		for (auto ele : map_flavor_num) 
		{
			const char *k= ele.first.c_str();
			// printf("%s\n", k );
			posi += sprintf(result_file + posi, " %s", k);
			posi += sprintf(result_file + posi, " %d", ele.second);
		}
	}
	// printf("%s\n", result_file);
	write_result(result_file, filename);	
}
#endif
