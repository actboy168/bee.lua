#pragma once

#include <Windows.h>
#include <string>
#include <map>
#include <memory>
#include <filesystem>
#include <thread>
#include <bee/utility/lockqueue.h>

namespace fs = std::filesystem;

namespace bee::win::fsevent {
	class task;

	typedef int taskid;
	enum class tasktype {
		Error,
		Create,
		Delete,
		Modify,
		Rename,
	};
	struct notify {
		tasktype     type;
		std::wstring path;
	};
	static const taskid kInvalidTaskId = 0;

	class watch {
		friend class task;
	public:

	public:
		watch();
		virtual ~watch();

		void   stop();
		taskid add(const std::wstring& path);
		bool   remove(taskid id);
		bool   select(notify& notify);

	public:
		void   apc_cb();

	private:
		struct apc_arg {
			enum class type {
				Add,
				Remove,
				Terminate,
			};
			type                  m_type;
			taskid                m_id;
			std::wstring          m_path;
		};
		void apc_add(taskid id, const std::wstring& path);
		void apc_remove(taskid id);
		void apc_terminate();
		void removetask(task* task);
		bool thread_init();
		bool thread_signal();
		void thread_cb();

	private:
		std::unique_ptr<std::thread>            m_thread;
		std::map<taskid, std::shared_ptr<task>> m_tasks;
		lockqueue<apc_arg>                      m_apc_queue;
		lockqueue<notify>                       m_notify;
		bool                                    m_terminate;
		taskid                                  m_gentask;
	};
}
