/*
 *	PROGRAM:		Firebird samples.
 *	MODULE:			CryptKeyHolder.cpp
 *	DESCRIPTION:	Sample of how key holder may be written.
 *
 *  The contents of this file are subject to the Initial
 *  Developer's Public License Version 1.0 (the "License");
 *  you may not use this file except in compliance with the
 *  License. You may obtain a copy of the License at
 *  http://www.ibphoenix.com/main.nfs?a=ibphoenix&page=ibp_idpl.
 *
 *  Software distributed under the License is distributed AS IS,
 *  WITHOUT WARRANTY OF ANY KIND, either express or implied.
 *  See the License for the specific language governing rights
 *  and limitations under the License.
 *
 *  The Original Code was created by Alex Peshkov
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2012 Alex Peshkov <peshkoff at mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "../interfaces/ifaceExamples.h"
#include <atomic>

namespace
{

IMaster* master = NULL;

class PluginModule : public IPluginModuleImpl<PluginModule, CheckStatusWrapper>
{
public:
	PluginModule()
		: pluginManager(NULL)
	{ }

	~PluginModule()
	{
		if (pluginManager)
		{
			pluginManager->unregisterModule(this);
			doClean();
		}
	}

	void registerMe(IPluginManager* m)
	{
		pluginManager = m;
		pluginManager->registerModule(this);
	}

	void doClean()
	{
		pluginManager = NULL;
	}

	void threadDetach() {}

private:
	IPluginManager* pluginManager;
};

class CryptKeyHolder : public IKeyHolderPluginImpl<CryptKeyHolder, CheckStatusWrapper>
{
public:
	explicit CryptKeyHolder(IPluginConfig* cnf) noexcept
		: callbackInterface(this), named(NULL), tempStatus(master->getStatus()),
		  config(cnf), key(0), init(false), owner(NULL)
	{
		config->addRef();
	}

	~CryptKeyHolder()
	{
		config->release();
		tempStatus.dispose();
	}

	// IKeyHolderPlugin implementation
	int keyCallback(CheckStatusWrapper* status, ICryptKeyCallback* callback);
	ICryptKeyCallback* keyHandle(CheckStatusWrapper* status, const char* keyName);
	ICryptKeyCallback* chainHandle(CheckStatusWrapper* status);

	int release()
	{
		if (--refCounter == 0)
		{
			delete this;
			return 0;
		}
		return 1;
	}

	void addRef()
	{
		++refCounter;
	}

	void setOwner(Firebird::IReferenceCounted* o)
	{
		owner = o;
	}

	IReferenceCounted* getOwner()
	{
		return owner;
	}

	const ISC_UCHAR& getKey()
	{
		if (!init)
		{
			keyCallback(&tempStatus, NULL);
			init = true;
		}

		return key;
	}

	FB_BOOLEAN useOnlyOwnKeys(CheckStatusWrapper* status)
	{
		IConfigEntry* e = getEntry(status, "OnlyOwnKey");
		if (!e)
			return FB_TRUE;	// safe default

		FB_BOOLEAN rc = e->getBoolValue();
		e->release();
		return rc;
	}

private:
	class CallbackInterface : public ICryptKeyCallbackImpl<CallbackInterface, CheckStatusWrapper>
	{
	public:
		explicit CallbackInterface(CryptKeyHolder* p)
			: holder(p)
		{ }

		unsigned int callback(unsigned int, const void*, unsigned int length, void* buffer) override
		{
			const ISC_UCHAR& k = holder->getKey();
			if (!k)
			{
				return 0;
			}

			if (length > 0 && buffer)
			{
				memcpy(buffer, &k, 1);
			}
			return 1;
		}

		int getHashLength(Firebird::CheckStatusWrapper* status) override
		{
			const ISC_UCHAR& k = holder->getKey();
			if (!k)
			{
				ISC_STATUS err[] = {isc_arg_gds, isc_wish_list};
				status->setErrors2(2, err);

				return -1;
			}

			return 1;
		}

		void getHashData(Firebird::CheckStatusWrapper* status, void* h) override
		{
			// here key value is returned by hash function as is
			// do not do it in production - use some hash function
			const ISC_UCHAR& k = holder->getKey();
			if (!k)
			{
				ISC_STATUS err[] = {isc_arg_gds, isc_wish_list};
				status->setErrors2(2, err);

				return;
			}

			memcpy(h, &k, 1);
		}

	private:
		CryptKeyHolder* holder;
	};

	class NamedCallback : public ICryptKeyCallbackImpl<NamedCallback, CheckStatusWrapper>
	{
	public:
		NamedCallback(NamedCallback* n, const char* nm, ISC_UCHAR k)
			: next(n), key(k)
		{
			strncpy(name, nm, sizeof(name));
			name[sizeof(name) - 1] = 0;
		}

		unsigned int callback(unsigned int, const void*, unsigned int length, void* buffer) override
		{
			memcpy(buffer, &key, 1);
			return 1;
		}

		int getHashLength(Firebird::CheckStatusWrapper* status) override
		{
			return 1;
		}

		void getHashData(Firebird::CheckStatusWrapper* status, void* h) override
		{
			// here key value is returned by hash function as is
			// do not do it in production - use some hash function
			memcpy(h, &key, 1);
		}

		~NamedCallback()
		{
			delete next;
		}

		char name[32];
		NamedCallback* next;
		ISC_UCHAR key;
	};

	CallbackInterface callbackInterface;
	NamedCallback *named;
	CheckStatusWrapper tempStatus;

	IPluginConfig* config;
	ISC_UCHAR key;
	bool init;

	std::atomic_int refCounter;
	IReferenceCounted* owner;

	IConfigEntry* getEntry(CheckStatusWrapper* status, const char* entryName);
};

IConfigEntry* CryptKeyHolder::getEntry(CheckStatusWrapper* status, const char* entryName)
{
	IConfig* def = config->getDefaultConfig(status);
	if (status->getState() & Firebird::IStatus::STATE_ERRORS)
		return NULL;

	IConfigEntry* confEntry = def->find(status, entryName);
	def->release();
	if (status->getState() & Firebird::IStatus::STATE_ERRORS)
		return NULL;

	return confEntry;
}

int CryptKeyHolder::keyCallback(CheckStatusWrapper* status, ICryptKeyCallback* callback)
{
	if (key != 0)
		return 1;

	IConfigEntry* confEntry = getEntry(status, "Auto");

	if (confEntry)
	{
		FB_BOOLEAN b = confEntry->getBoolValue();
		confEntry->release();

		if (b)
		{
			confEntry = getEntry(status, "Key");
			if (confEntry)
			{
				key = static_cast<ISC_UCHAR>(confEntry->getIntValue());
				confEntry->release();
			}
			else
				key = 0x5a;

			return 1;
		}
	}

	if (callback && callback->callback(0, NULL, 1, &key) != 1)
	{
		key = 0;
		return 0;
	}

	return 1;
}

ICryptKeyCallback* CryptKeyHolder::keyHandle(CheckStatusWrapper* status, const char* keyName)
{
	if (keyName[0] == 0)
		return &callbackInterface;

	for (NamedCallback* n = named; n; n = n->next)
	{
		if (strcmp(keyName, n->name) == 0)
			return n;
	}

	char kn[40];
	strcpy(kn, "Key");
	strncat(kn, keyName, sizeof(kn) - 3 - 1);
	kn[sizeof(kn) - 1] = 0;

	IConfigEntry* confEntry = getEntry(status, kn);
	if (confEntry)
	{
		int k = static_cast<int>(confEntry->getIntValue());
		confEntry->release();
		if (k > 0 && k < 256)
		{
			named = new NamedCallback(named, keyName, static_cast<ISC_UCHAR>(k));
			return named;
		}
	}

	return NULL;
}

ICryptKeyCallback* CryptKeyHolder::chainHandle(CheckStatusWrapper* status)
{
	return &callbackInterface;
}


class Factory : public IPluginFactoryImpl<Factory, CheckStatusWrapper>
{
public:
	IPluginBase* createPlugin(CheckStatusWrapper* status, IPluginConfig* factoryParameter)
	{
		CryptKeyHolder* p = new CryptKeyHolder(factoryParameter);
		p->addRef();
		return p;
	}
};

PluginModule module;
Factory factory;

} // anonymous namespace

extern "C" FB_DLL_EXPORT void FB_PLUGIN_ENTRY_POINT(IMaster* m)
{
	master = m;
	IPluginManager* pluginManager = master->getPluginManager();

	module.registerMe(pluginManager);
	pluginManager->registerPluginFactory(IPluginManager::TYPE_KEY_HOLDER, "fbSampleKeyHolder",
		&factory);
}

