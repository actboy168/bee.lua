#include <bee/filewatch/filewatch_win.h>
#include <bee/utility/unicode.h>
#include <bee/utility/format.h>
#include <bee/exception/windows_exception.h>
#include <process.h>
#include <assert.h>

namespace bee::win {
	static void __stdcall filewatch_apc_cb(ULONG_PTR arg) {
		filewatch* self = (filewatch*)arg;
		self->apc_cb();
	}

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
		std::error_code ec;
		m_path = fs::absolute(path, ec);
		if (ec) {
			push_notify(tasktype::Error, bee::format(L"`std::filesystem::absolute` failed: %s", ec.message()));
			return false;
		}
		DWORD sharemode = FILE_SHARE_READ | FILE_SHARE_WRITE;
		m_directory = ::CreateFileW(m_path.c_str(),
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
			&task::changes_cb)) 
		{
			push_notify(tasktype::Error, bee::format(L"`ReadDirectoryChangesW` failed: %s", error_message()));
			return false;
		}
		return true;
	}

	void filewatch::task::changes_cb(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped) {
		task* self = (task*)lpOverlapped->hEvent;
		self->changes_cb(dwErrorCode, dwNumberOfBytesTransfered);
	}

	void filewatch::task::changes_cb(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered) {
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
		: m_thread()
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

	bool filewatch::thread_signal() {
		return !!::QueueUserAPC(filewatch_apc_cb, m_thread->native_handle(), (ULONG_PTR)this);
	}

	bool filewatch::thread_init() {
		if (m_thread) {
			return true;
		}
		m_thread.reset(new std::thread(std::bind(&filewatch::thread_cb, this)));
		return true;
	}

	void filewatch::thread_cb() {
		while (!m_terminate || !m_tasks.empty()) {
			::SleepEx(INFINITE, true);
		}
	}

	void filewatch::stop() {
		if (!m_thread) {
			return;
		}
		if (!m_thread->joinable()) {
			m_thread.reset();
			return;
		}
		m_apc_queue.push({
			apc_arg::type::Terminate
		});
		if (!thread_signal()) {
			m_thread->detach();
			m_thread.reset();
			return;
		}
		m_thread->join();
		m_thread.reset();
	}

	filewatch::taskid filewatch::add(const std::wstring& path) {
		if (!thread_init()) {
			return kInvalidTaskId;
		}
		taskid id = ++m_gentask;
		m_apc_queue.push({
			apc_arg::type::Add,
			id,
			path
		});
		thread_signal();
		return id;
	}

	bool filewatch::remove(taskid id) {
		if (!m_thread) {
			return false;
		}
		m_apc_queue.push({
			apc_arg::type::Remove,
			id
		});
		thread_signal();
		return true;
	}

	void filewatch::apc_cb() {
		apc_arg arg;
		while (m_apc_queue.pop(arg)) {
			switch (arg.m_type) {
			case apc_arg::type::Add:
				apc_add(arg.m_id, arg.m_path);
				break;
			case apc_arg::type::Remove:
				apc_remove(arg.m_id);
				break;
			case apc_arg::type::Terminate:
				apc_terminate();
				return;
			}
		}
	}

	void filewatch::apc_add(taskid id, const std::wstring& path) {
		auto t = std::make_shared<task>(this, id);
		if (!t->open(path)) {
			return;
		}
		m_tasks.insert(std::make_pair(id, t));
		t->start();
	}

	void filewatch::apc_remove(taskid id) {
		auto it = m_tasks.find(id);
		if (it != m_tasks.end()) {
			it->second->cancel();
		}
	}

	void filewatch::apc_terminate() {
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
		m_terminate = true;
	}

	bool filewatch::select(notify& n) {
		return m_notify.pop(n);
	}
}
