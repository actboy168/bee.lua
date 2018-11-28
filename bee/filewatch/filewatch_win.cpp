#include <bee/filewatch/filewatch_win.h>
#include <bee/utility/unicode.h>
#include <bee/utility/format.h>
#include <bee/exception/windows_exception.h>
#include <process.h>
#include <assert.h>

namespace bee::win {
	filewatch::task::task(filewatch* watch, taskid id)
		: m_watch(watch)
		, m_id(id)
		, m_path()
		, m_directory(INVALID_HANDLE_VALUE)
	{
		memset(this, 0, sizeof(OVERLAPPED));
		hEvent = this;
	}

	filewatch::task::~task() {
		assert(m_directory == INVALID_HANDLE_VALUE);
	}

	bool filewatch::task::open(const std::wstring& path) {
		if (m_directory != INVALID_HANDLE_VALUE) {
			return true;
		}
		DWORD sharemode = FILE_SHARE_READ | FILE_SHARE_WRITE;
		m_directory = ::CreateFileW(path.c_str(),
			FILE_LIST_DIRECTORY,
			sharemode,
			NULL,
			OPEN_EXISTING,
			FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
			NULL);
		if (m_directory == INVALID_HANDLE_VALUE) {
			push_notify(tasktype::Error, bee::format(L"`CreateFileW` failed: %s", error_message()));
			return false;
		}
		m_path = path;
		return true;
	}

	void filewatch::task::cancel() {
		if (m_directory != INVALID_HANDLE_VALUE) {
			::CancelIo(m_directory);
			::CloseHandle(m_directory);
			m_directory = INVALID_HANDLE_VALUE;
		}
	}

	filewatch::taskid filewatch::task::task::getid() {
		return m_id;
	}

	void filewatch::task::remove() {
		if (m_watch) {
			m_watch->removetask(this);
		}
	}

	bool filewatch::task::start() {
		assert(m_directory != INVALID_HANDLE_VALUE);
		if (!::ReadDirectoryChangesW(
			m_directory,
			&m_buffer[0],
			static_cast<DWORD>(m_buffer.size()),
			TRUE,
			FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
			FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_LAST_ACCESS | FILE_NOTIFY_CHANGE_CREATION,
			NULL,
			this,
			&task::proc_changes)) 
		{
			push_notify(tasktype::Error, bee::format(L"`ReadDirectoryChangesW` failed: %s", error_message()));
			return false;
		}
		return true;
	}

	void filewatch::task::proc_changes(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped) {
		task* t = (task*)lpOverlapped->hEvent;
		t->proc_changes(dwErrorCode, dwNumberOfBytesTransfered);
	}

	void filewatch::task::proc_changes(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered) {
		if (dwErrorCode == ERROR_OPERATION_ABORTED) {
			remove();
			return;
		}
		if (!dwNumberOfBytesTransfered) {
			return;
		}
		assert(dwNumberOfBytesTransfered >= offsetof(FILE_NOTIFY_INFORMATION, FileName) + sizeof(WCHAR));
		assert(dwNumberOfBytesTransfered <= m_bakbuffer.size());
		memcpy(&m_bakbuffer[0], &m_buffer[0], dwNumberOfBytesTransfered);
		start();

		uint8_t* data = m_bakbuffer.data();
		for (;;) {
			FILE_NOTIFY_INFORMATION& fni = (FILE_NOTIFY_INFORMATION&)*data;
			std::wstring path(fni.FileName, fni.FileNameLength / sizeof(wchar_t));
			switch (fni.Action) {
			case FILE_ACTION_ADDED:
				push_notify(tasktype::Create, (m_path / path).wstring());
				break;
			case FILE_ACTION_REMOVED:
				push_notify(tasktype::Delete, (m_path / path).wstring());
				break;
			case FILE_ACTION_MODIFIED:
				push_notify(tasktype::Modify, (m_path / path).wstring());
				break;
			case FILE_ACTION_RENAMED_OLD_NAME:
			case FILE_ACTION_RENAMED_NEW_NAME:
				push_notify(tasktype::Rename, (m_path / path).wstring());
				break;
			default:
				assert(false);
				break;
			}
			if (!fni.NextEntryOffset) {
				break;
			}
			data += fni.NextEntryOffset;
		}
	}

	void filewatch::task::push_notify(tasktype type, std::wstring&& message) {
		m_watch->m_notify.push({
			type,
			std::forward<std::wstring>(message),
		});
	}

	filewatch::filewatch()
		: m_thread(NULL)
		, m_tasks()
		, m_notify()
		, m_terminate(false)
		, m_gentask(kInvalidTaskId)
	{ }

	filewatch::~filewatch() {
		stop();
		assert(m_tasks.empty());
	}

	void filewatch::removetask(task* t) {
		if (t) {
			auto it = m_tasks.find(t->getid());
			if (it != m_tasks.end()) {
				m_tasks.erase(it);
			}
		}
	}

	void filewatch::stop() {
		if (!m_thread) {
			return;
		}
		m_terminate = true;
		std::unique_ptr<apc_arg> arg(new apc_arg);
		arg->m_watch = this;
		arg->m_type = apc_arg::type::Terminate;
		bool ok = !!::QueueUserAPC(filewatch::proc_apc, m_thread, (ULONG_PTR)arg.get());
		if (ok) {
			arg.release();
		}
		::WaitForSingleObject(m_thread, INFINITE);
		::CloseHandle(m_thread);
		m_thread = NULL;
	}

	filewatch::taskid filewatch::add(const std::wstring& path) {
		::SetLastError(0);
		bool ok = true;
		if (!m_thread) {
			m_thread = (HANDLE)_beginthreadex(NULL, 0, filewatch::proc_thread, this, 0, NULL);
			if (!m_thread) {
				return kInvalidTaskId;
			}
		}
		taskid id = ++m_gentask;
		std::unique_ptr<apc_arg> arg(new apc_arg);
		arg->m_watch = this;
		arg->m_type = apc_arg::type::Add;
		arg->m_id = id;
		arg->m_path = path;
		ok = !!::QueueUserAPC(filewatch::proc_apc, m_thread, (ULONG_PTR)arg.get());
		if (!ok) {
			return kInvalidTaskId;
		}
		arg.release();
		return id;
	}

	bool filewatch::remove(taskid id) {
		if (!m_thread) {
			return false;
		}
		std::unique_ptr<apc_arg> arg(new apc_arg);
		arg->m_watch = this;
		arg->m_type = apc_arg::type::Remove;
		arg->m_id = id;
		bool ok = !!::QueueUserAPC(filewatch::proc_apc, m_thread, (ULONG_PTR)arg.get());
		if (ok) {
			arg.release();
		}
		return ok;
	}

	unsigned int filewatch::proc_thread(void* arg) {
		filewatch* self = (filewatch*)arg;
		while (!self->m_terminate || !self->m_tasks.empty()) {
			::SleepEx(INFINITE, true);
		}
		return 0;
	}

	void filewatch::proc_apc(ULONG_PTR ptr) {
		std::unique_ptr<apc_arg> arg((apc_arg*)ptr);
		switch (arg->m_type) {
		case apc_arg::type::Add:
			arg->m_watch->proc_add(arg.get());
			break;
		case apc_arg::type::Remove:
			arg->m_watch->proc_remove(arg.get());
			break;
		case apc_arg::type::Terminate:
			arg->m_watch->proc_terminate(arg.get());
			break;
		}
	}

	void filewatch::proc_add(apc_arg* arg) {
		auto t = std::make_shared<task>(this, arg->m_id);
		if (!t->open(arg->m_path)) {
			return;
		}
		m_tasks.insert(std::make_pair(arg->m_id, t));
		t->start();
	}

	void filewatch::proc_remove(apc_arg* arg) {
		auto it = m_tasks.find(arg->m_id);
		if (it != m_tasks.end()) {
			it->second->cancel();
		}
	}

	void filewatch::proc_terminate(apc_arg* arg) {
		if (m_tasks.empty()) {
			return;
		}
		std::vector<std::shared_ptr<task>> tmp;
		for (auto& it : m_tasks) {
			tmp.push_back(it.second);
		}
		for (auto& it : tmp) {
			it->cancel();
		}
	}

	bool filewatch::select(notify& n) {
		return m_notify.pop(n);
	}
}
