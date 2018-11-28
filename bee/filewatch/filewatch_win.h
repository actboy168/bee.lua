#pragma once

#include <Windows.h>
#include <string>
#include <array>
#include <map>
#include <memory>
#include <bee/utility/lockqueue.h>

namespace bee::win {
	class filewatch {
	public:
		typedef int taskid;
		enum class tasktype {
			Error = 0,
			Create = 1,
			Delete,
			Modify,
			RenameFrom,
			RenameTo,
		};

		struct notify {
			taskid       id;
			tasktype     type;
			std::wstring message;
		};

		static const taskid kInvalidTaskId = 0;
		static const int WatchFile     = 0x0001;
		static const int WatchDir      = 0x0002;
		static const int WatchSubtree  = 0x0004;
		static const int WatchTime     = 0x0008;
		static const int DisableDelete = 0x0010;

	public:
		filewatch();
		virtual ~filewatch();

		void   stop();
		taskid add(const std::wstring& directory, int filter);
		bool   remove(taskid id);
		bool   pop(notify& notify);

	private:
		class task : public OVERLAPPED {
			static const size_t kBufSize = 16 * 1024;

		public:
			task(filewatch* watch, taskid id, int filter);
			~task();

			bool   open(const std::wstring& directory);
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
			HANDLE                        m_directory;
			int                           m_flag;
			std::array<uint8_t, kBufSize> m_buffer;
			std::array<uint8_t, kBufSize> m_bakbuffer;
		};

	private:
		struct apcarg {
			enum class Type {
				Add,
				Remove,
				Terminate,
			};
			filewatch*            m_watch;
			Type                  m_type;
			taskid                m_id;
			std::wstring          m_directory;
			int                   m_filter;
		};
		static unsigned int __stdcall proc_thread(void* arg);
		static void         __stdcall proc_apc(ULONG_PTR arg);
		void proc_add(apcarg* arg);
		void proc_remove(apcarg* arg);
		void proc_terminate(apcarg* arg);
		void removetask(task* task);

	private:
		HANDLE                                  m_thread;
		std::map<taskid, std::shared_ptr<task>> m_tasks;
		bee::lockqueue<notify>                  m_notify;
		bool                                    m_terminate;
		taskid                                  m_gentask;
	};
}
