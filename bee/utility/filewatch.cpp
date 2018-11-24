#include <bee/utility/filewatch.h>
#include <bee/utility/unicode.h>
#include <bee/exception/windows_exception.h>
#include <process.h>
#include <assert.h>

namespace bee {
	filewatch::task::task(filewatch* watch, taskid id, int filter)
		: m_watch(watch)
		, m_id(id)
		, m_directory(INVALID_HANDLE_VALUE)
		, m_flag(filter)
	{
		memset(this, 0, sizeof(OVERLAPPED));
		hEvent = this;
	}

	filewatch::task::~task() {
		assert(m_directory == INVALID_HANDLE_VALUE);
	}

	bool filewatch::task::open(const std::wstring& directory) {
		if (m_directory != INVALID_HANDLE_VALUE) {
			return true;
		}
		DWORD sharemode = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
		if (m_flag & DisableDelete) {
			sharemode &= ~FILE_SHARE_DELETE;
		}
		m_directory = ::CreateFileW(directory.c_str(),
			FILE_LIST_DIRECTORY,
			sharemode,
			NULL,
			OPEN_EXISTING,
			FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
			NULL);
		bool ok = (m_directory != INVALID_HANDLE_VALUE);
		if (ok) {
			return true;
		}
		push_notify(tasktype::Error, error_message());
		return false;
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
		DWORD flags = 0;
		if (m_flag & WatchFile) {
			flags |= FILE_NOTIFY_CHANGE_FILE_NAME;
		}
		if (m_flag & WatchDir) {
			flags |= FILE_NOTIFY_CHANGE_DIR_NAME;
		}
		if (m_flag & WatchTime) {
			flags |= FILE_NOTIFY_CHANGE_LAST_WRITE
				| FILE_NOTIFY_CHANGE_LAST_WRITE;
		}
		bool ok = !!::ReadDirectoryChangesW(
			m_directory,
			&m_buffer[0],
			static_cast<DWORD>(m_buffer.size()),
			(m_flag & WatchSubtree) != 0,
			flags,
			NULL,
			this,
			&task::proc_changes);
		if (ok) {
			return true;
		}
		push_notify(tasktype::Error, error_message());
		return false;
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
				push_notify(tasktype::Create, std::move(path));
				break;
			case FILE_ACTION_REMOVED:
				push_notify(tasktype::Delete, std::move(path));
				break;
			case FILE_ACTION_MODIFIED:
				push_notify(tasktype::Modify, std::move(path));
				break;
			case FILE_ACTION_RENAMED_OLD_NAME:
				push_notify(tasktype::RenameFrom, std::move(path));
				break;
			case FILE_ACTION_RENAMED_NEW_NAME:
				push_notify(tasktype::RenameTo, std::move(path));
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
			m_id,
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
		std::unique_ptr<apcarg> arg(new apcarg);
		arg->m_watch = this;
		arg->m_type = apcarg::Type::Terminate;
		bool ok = !!::QueueUserAPC(filewatch::proc_apc, m_thread, (ULONG_PTR)arg.get());
		if (ok) {
			arg.release();
		}
		::WaitForSingleObject(m_thread, INFINITE);
		::CloseHandle(m_thread);
		m_thread = NULL;
	}

	filewatch::taskid filewatch::add(const std::wstring& directory, int filter) {
		::SetLastError(0);
		bool ok = true;
		if (!m_thread) {
			m_thread = (HANDLE)_beginthreadex(NULL, 0, filewatch::proc_thread, this, 0, NULL);
			if (!m_thread) {
				return kInvalidTaskId;
			}
		}
		taskid id = ++m_gentask;
		std::unique_ptr<apcarg> arg(new apcarg);
		arg->m_watch = this;
		arg->m_type = apcarg::Type::Add;
		arg->m_id = id;
		arg->m_directory = directory;
		arg->m_filter = filter;
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
		std::unique_ptr<apcarg> arg(new apcarg);
		arg->m_watch = this;
		arg->m_type = apcarg::Type::Remove;
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
		std::unique_ptr<apcarg> arg((apcarg*)ptr);
		switch (arg->m_type) {
		case apcarg::Type::Add:
			arg->m_watch->proc_add(arg.get());
			break;
		case apcarg::Type::Remove:
			arg->m_watch->proc_remove(arg.get());
			break;
		case apcarg::Type::Terminate:
			arg->m_watch->proc_terminate(arg.get());
			break;
		}
	}

	void filewatch::proc_add(apcarg* arg) {
		auto t = std::make_shared<task>(this, arg->m_id, arg->m_filter);
		if (!t->open(arg->m_directory)) {
			return;
		}
		m_tasks.insert(std::make_pair(arg->m_id, t));
		t->start();
	}

	void filewatch::proc_remove(apcarg* arg) {
		auto it = m_tasks.find(arg->m_id);
		if (it != m_tasks.end()) {
			it->second->cancel();
		}
	}

	void filewatch::proc_terminate(apcarg* arg) {
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

	bool filewatch::pop(notify& n) {
		return m_notify.pop(n);
	}
}
