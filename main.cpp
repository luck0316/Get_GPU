#include "get.h"
#include <iostream>
#include <iomanip>

struct HelpInfo {
	std::vector<std::string> options = {
		"-p ���̺�\t����ָ�����̵�GPU���",
		"-h\t\t��ʾ������Ϣ"
	};
};

int main(int argc, char** argv)
{
	HelpInfo help_info;

	for (int i = 0; i < argc; ++i) {

		if (argc < 2 || std::string(argv[1]) == "-h") {
			for (const auto& opt : help_info.options) {
				std::cout << opt << std::endl;
			}
			return 0;
		}

	}

	if (std::string(argv[1]) != "-p" || argc < 3) {
		std::cout << "����������в�������ʹ�� -p ���̺� ָ��Ҫ���ӵĽ��̺š�\n";
		return -1;
	}

	DWORD pid = std::stoul(argv[2]);

	GpuMonitor monitor(pid);
	if (!monitor.start())
	{
		std::cout << "monitor.start failed.\n";
		return -1;
	}
	while (true)
	{
		// ����ϵͳ����Ϣ
		monitor.EtpUpdateSystemSegmentInformation();
		std::cout << "GPU dedicated usage: " << monitor.GPU_memory_used() << "MB" << std::endl;
		std::cout << "GPU dedicated: " << monitor.GPU_memory() << "MB" << std::endl;
		std::cout << "GPU dedicated usage: " << std::fixed << std::setprecision(1) << monitor.GPU_memory_used()/1024 << "GB" << std::endl;
		std::cout << "GPU dedicated: " << std::fixed << monitor.GPU_memory()/1024 << "GB" << std::endl;
		break;
	}

	return 0;
}
