#include "get.h"
#include <iostream>
#include <iomanip>

struct HelpInfo {
	std::vector<std::string> options = {
		"-p 进程号\t监视指定进程的GPU情况",
		"-h\t\t显示帮助信息"
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
		std::cout << "错误的命令行参数。请使用 -p 进程号 指定要监视的进程号。\n";
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
		// 更新系统段信息
		monitor.EtpUpdateSystemSegmentInformation();
		std::cout << "GPU dedicated usage: " << monitor.GPU_memory_used() << "MB" << std::endl;
		std::cout << "GPU dedicated: " << monitor.GPU_memory() << "MB" << std::endl;
		std::cout << "GPU dedicated usage: " << std::fixed << std::setprecision(1) << monitor.GPU_memory_used()/1024 << "GB" << std::endl;
		std::cout << "GPU dedicated: " << std::fixed << monitor.GPU_memory()/1024 << "GB" << std::endl;
		break;
	}

	return 0;
}
