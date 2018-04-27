#ifndef __SERVER_H__
#define __SERVER_H__

#include <string>
#include <vector>

//Flavor�࣬�����������Ϣ������ �ڴ� CPU
struct Flavor {
	std::string name;
	int mem;
	
	int cpu;
	Flavor(std::string _name,int _mem, int _cpu):
		name(_name),mem(_mem),cpu(_cpu){}
	Flavor() {

	}
};

//Server�࣬���������������Ϣ�����ڴ� ��CPU �����ڴ� ����CPU �Ѵ�ŵ�������б�
class Server {
public:
	std::vector<Flavor> flavors;  //����������Ѵ��������б�
	///Server���캯��������Ϊ�ڴ��С��CPU��С
	Server(int mem, int cpu);
	///�������������������Ϊ��������󣬷���ֵΪ�Ƿ���óɹ�
	bool put_flavor(Flavor flavor);
	///��ȡ������CPUʹ����
	double get_cpu_usage_rate();
	///��ȡ�������ڴ�ʹ����
	double get_mem_usage_rate();
private:
	int total_mem;  // ������������ڴ�
	int total_cpu;  // �����������CPU
	int free_mem;   // ���������ʣ������ڴ�
	int free_cpu;   // ���������ʣ�����CPU
};
#endif  //__SERVER_H__