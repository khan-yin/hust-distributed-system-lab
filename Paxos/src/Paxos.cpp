#include <stdlib.h>
#include <stdio.h>
#include "Paxos/Acceptor.h"
#include "Paxos/Proposer.h"
#include "lib/Thread.h"
#include "lib/Lock.h"
#include "lib/mapi.h"
#include "lib/atom.h"
#include "lib/Logger.h"

paxos::Proposer p[5];
paxos::Acceptor a[11];
mdk::Mutex l[11];
int finishedCount = 0;
int finalValue = -1;
bool isFinished = false;
mdk::uint64 g_start;
mdk::Logger g_log;

void* Proposer(void *id)
{
	mdk::Logger log;
	char logName[256];
	sprintf( logName, "Proposer%d", (long)id );
	log.SetLogName(logName);
	log.SetMaxLogSize(10);
	log.SetMaxExistDay(30);
	log.SetPrintLog(false);

	paxos::Proposer &proposer = p[(long)id];
	paxos::PROPOSAL value = proposer.GetProposal();
	paxos::PROPOSAL lastValue;


	int acceptorId[11];
	int count = 0;

	mdk::uint64 start = mdk::MillTime();
	while ( true )
	{
		value = proposer.GetProposal();//�õ�����
		log.Info("Info", "Proposer%d�ſ�ʼ(Propose�׶�):����=[���:%d������:%d]\n", 
			(long)id, value.serialNum, value.value);
		count = 0;
		int i = 0;
		for (i = 0; i < 11; i++ )
		{
		/*
			* ������Ϣ����i��acceptor
			* ����һ��ʱ��ﵽacceptor��sleep(���ʱ��)ģ��
			* acceptor������Ϣ��mAcceptors[i].Propose()
			* ��Ӧproposer
			* ����һ��ʱ��proposer�յ���Ӧ��sleep(���ʱ��)ģ��
			* proposer�����ӦmProposer.proposed(ok, lastValue)
		*/
			mdk::m_sleep(rand()%500);//�������ʱ�䣬��Ϣ������mAcceptors[i]
			//������Ϣ
			l[i].Lock();
			bool ok = a[i].Propose(value.serialNum, lastValue);
			l[i].Unlock();
			mdk::m_sleep(rand()%500);//�������ʱ��,��Ϣ����Proposer
			//����Propose��Ӧ
			if ( !proposer.Proposed(ok, lastValue) ) //���¿�ʼPropose�׶�
			{
				mdk::m_sleep(1000);//Ϊ�˽��ͻ��������һ���ñ��proposer�л�������Լ���2�׶���׼
				break;
			}
			paxos::PROPOSAL curValue = proposer.GetProposal();//�õ�����
			if ( curValue.value != value.value )//acceptor���λ�Ӧ�����Ƽ���һ������
			{
				log.Info("Info", "Proposer%d���޸�������:����=[���:%d������:%d]\n", 
					(long)id, curValue.serialNum, curValue.value);
				break;
			}
			acceptorId[count++] = i;//��¼Ը��ͶƱ��acceptor
			if ( proposer.StartAccept() ) break;
		}
		//�����û�дﵽAccept��ʼ���������û�б�ʾҪ���¿�ʼPropose�׶�
		if ( !proposer.StartAccept() ) continue;

		//��ʼAccept�׶�
		//����Accept��Ϣ������Ը��ͶƱ��acceptor
		value = proposer.GetProposal();
		log.Info("Info", "Proposer%d�ſ�ʼ(Accept�׶�):����=[���:%d������:%d]\n", 
			(long)id, value.serialNum, value.value);
		for (i = 0; i < count; i++ )
		{
			//����accept��Ϣ��acceptor
			//����accept�׶εȴ�ʱ�䣬�ӿ�����
			mdk::m_sleep(rand()%200);//�������ʱ��,accept��Ϣ����acceptor
			//����accept��Ϣ
			l[acceptorId[i]].Lock();
			bool ok = a[acceptorId[i]].Accept(value);
			l[acceptorId[i]].Unlock();
			mdk::m_sleep(rand()%200);//�������ʱ��,accept��Ӧ����proposer
			//����accept��Ӧ
			if ( !proposer.Accepted(ok) ) //���¿�ʼPropose�׶�
			{
				mdk::m_sleep(1000);//Ϊ�˽��ͻ��������һ���ñ��proposer�л�������Լ���2�׶���׼
				break;
			}
			if ( proposer.IsAgree() )//�ɹ���׼������
			{
				start = mdk::MillTime() - start;
				log.Info("Info", "Proposer%d�ŵ����鱻��׼,��ʱ%lluMS:�������� = [���:%d������:%d]\n", (long)id, start, value.serialNum, value.value);
				g_log.Info("Info", "Proposer%d�ŵ����鱻��׼,��ʱ%lluMS:�������� = [���:%d������:%d]\n", (long)id, start, value.serialNum, value.value);
				if(finalValue == -1) finalValue = value.value;
				else if(finalValue != value.value) finalValue = 0;
				if ( 4 == mdk::AtomAdd(&finishedCount, 1) )
				{
					isFinished = true;
					g_start = mdk::MillTime() - g_start;
					if(finalValue > 0){
						g_log.Info("Info", "Paxos��ɣ���ʱ%lluMS������ͨ������ֵΪ��%d\n", g_start, finalValue);
					}
					else{
						g_log.Info("Info", "Paxos��ɣ���ʱ%lluMS�����ս����һ�£�\n", g_start);
					}
				}
				return NULL;
			}
		}
	}
	return NULL;
}

//Paxos����ģ����ʾ����
int main(int argc, char* argv[])
{
	int i = 0;
	g_log.SetLogName("Paxos");
	g_log.SetMaxLogSize(10);
	g_log.SetMaxExistDay(30);
	g_log.SetPrintLog(true);
	g_log.Info("Info", "5��Proposer, 11��Acceptor׼������Paxos\n"
		"ÿ��Proposer�����̣߳�Acceptor����Ҫ�߳�\n"
		"Proposer��Ŵ�0-10,���Ϊi��Proposer��ʼ�����ź�����ֵ�ǣ�i+1, i+1��\n"
		"Proposerÿ����������Ὣ����������5\n"
		"Proposer����׼������߳�,�����̼߳���ͶƱ���գ�ȫ����׼��ͬ��ֵ�����һ�¡�\n");
	g_start = mdk::MillTime();
	g_log.Info("Info", "Paxos��ʼ\n" );
	paxos::PROPOSAL value;

	for ( i = 0; i < 5; i++ ) 
	{
		p[i].SetPlayerCount(5, 11);
		value.serialNum = value.value = i + 1;
		p[i].StartPropose(value);
	}

	mdk::Thread t[5];
	for ( i = 0; i < 5; i++ ) t[i].Run(Proposer, (void*)i);
	//for ( i = 0; i < 5; i++ ) t[i].WaitStop();
	while(true){
		if(isFinished) break;
		mdk::m_sleep(500);
	}
	return 0;
}
