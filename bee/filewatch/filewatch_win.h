#pragma once

#include <Windows.h>
#include <string>
#include <array>
#include <map>
#include <memory>
#include <filesystem>
#include <bee/utility/lockqueue.h>

namespace fs = std::filesystem;

namespace bee::win {

	class filewatch {
	public:
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

	public:
		filewatch();
		virtual ~filewatch();

		void   stop();
		taskid add(const std::wstring& path);
		bool   remove(taskid id);
		bool   select(notify& notify);

	private:
		class task : public OVERLAPPED {
			static const size_t kBufSize = 16 * 1024;

		public:
			task(filewatch* watch, taskid id);
			~task();

			bool   open(const std::wstring& path);
			bool   start();
			void   cancel();
			taskid getid();
			void   push_notify(tasktype type, std::wstring&& message);

		private:
			void remove();
			static void __stdcall proc_changes(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped);
			void proc_changes(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered);

		private:
			filewatch*                    m_watch;
			taskid                        m_id;
			fs::path                      m_path;
			HANDLE                        m_directory;
			std::array<uint8_t, kBufSize> m_buffer;
			std::array<uint8_t, kBufSize> m_bakbuffer;
		};

	private:
		struct apc_arg {
			enum class type {
				Add,
				Remove,
				Terminate,
			};
			filewatch*            m_watch;
			type                  m_type;
			taskid                m_id;
			std::wstring          m_path;
		};
		static unsigned int __stdcall proc_thread(void* arg);
		static void         __stdcall proc_apc(ULONG_PTR arg);
		void proc_add(apc_arg* arg);
		void proc_remove(apc_arg* arg);
		void proc_terminate(apc_arg* arg);
		void removetask(task* task);

	private:
		HANDLE                                  m_thread;
		std::map<taskid, std::shared_ptr<task>> m_tasks;
		bee::lockqueue<notify>                  m_notify;
		bool                                    m_terminate;
		taskid                                  m_gentask;
	};
}
