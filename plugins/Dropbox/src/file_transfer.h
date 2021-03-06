#ifndef _FILE_TRANSFER_H_
#define _FILE_TRANSFER_H_

class TransferException
{
	CMStringA message;

public:
	TransferException(const char *message) :
		message(message)
	{
	}

	const char* what() const throw()
	{
		return message.c_str();
	}
};

struct FileTransferParam
{
	HANDLE hProcess;
	MCONTACT hContact;
	PROTOFILETRANSFERSTATUS pfts;

	bool isTerminated;

	int totalFolders;
	TCHAR **ptszFolders;
	int relativePathStart;

	LIST<char> urlList;

	FileTransferParam() : urlList(1)
	{
		totalFolders = 0;
		ptszFolders = NULL;
		relativePathStart = 0;

		hProcess = NULL;
		hContact = NULL;

		isTerminated = false;

		pfts.cbSize = sizeof(this->pfts);
		pfts.flags = PFTS_TCHAR | PFTS_SENDING;
		pfts.hContact = NULL;
		pfts.currentFileNumber = 0;
		pfts.currentFileProgress = 0;
		pfts.currentFileSize = 0;
		pfts.currentFileTime = 0;
		pfts.totalBytes = 0;
		pfts.totalFiles = 0;
		pfts.totalProgress = 0;
		pfts.pszFiles = NULL;
		pfts.tszWorkingDir = NULL;
		pfts.tszCurrentFile = NULL;
	}

	~FileTransferParam()
	{
		if (pfts.tszWorkingDir)
			mir_free(pfts.tszWorkingDir);

		if (pfts.pszFiles)
		{
			for (int i = 0; pfts.pszFiles[i]; i++)
			{
				if (pfts.pszFiles[i]) mir_free(pfts.pszFiles[i]);
			}
			mir_free(pfts.pszFiles);
		}

		if (ptszFolders)
		{
			for (int i = 0; ptszFolders[i]; i++)
			{
				if (ptszFolders[i]) mir_free(ptszFolders[i]);
			}
			mir_free(ptszFolders);
		}

		for (int i = 0; i < urlList.getCount(); i++)
			mir_free(urlList[i]);
		urlList.destroy();
	}

	void AddUrl(const char *url)
	{
		urlList.insert(mir_strdup(url));
	}
};

#endif //_FILE_TRANSFER_H_