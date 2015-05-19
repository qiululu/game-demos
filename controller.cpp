
// C++ includes
#include <map>
#include <string>
#include <sstream>
// test inclues
#include "AppDelegate.h"
#include "BaseTest.h"
#include "controller.h"

#if (CC_TARGET_PLATFORM != CC_PLATFORM_WIN32) && (CC_TARGET_PLATFORM != CC_PLATFORM_WP8) && (CC_TARGET_PLATFORM != CC_PLATFORM_WINRT)
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#else
#include <io.h>
#include <WS2tcpip.h>
#endif

#define		LINE_SPACE          40
#define		TILE_WIDTH			50
#define		TILE_HEIGHT			20
#define     MAX_POINT			9999
#define		CARD_UP_DIS			30
#define		MAX_LEVEL			7
#define		MAX_NUM				6
#define		LEFT_MOST			150
#define		RIGHT_MOST			850
#define		TIME_TICK			0.01f

int g_configEachLv[] = { 20, 70, 120, 300, 500, 1000, 5000};
float g_configRate[] = { 0.10, 0.07, 0.035, 0.015, 0.007, 0.003, 0.002 }; 

ui::Widget* m_mainLayer;
ui::Widget* m_mainLayerBattle;
Node* m_btnEnemy;
GLESDebugDraw *m_debugDraw;


//left pal
Node* m_imgLeft;
float m_IncRate;
int m_crntPoint;
int m_nextLvPoint;
int m_crntLevel;
Vec2 m_leftPos;
ui::Text* m_labNextPoint;
ui::Text* m_lab_point;
ui::Text* m_labLv;
//pals
stCard m_allCard[MAX_NUM];
std::map<b2Body*, int> g_mapDestBd;

#define SPAN 20
float g_timeBattle = 0;
int g_channel = 0;
std::vector<vSpawn*> g_vecEnemy;
std::vector<vSpawn*> g_vecHeros;
std::vector<bulletSpawn*> g_vecbullet;
b2World* g_world;
MyContactListener* contactListener;
BaseSpawn* g_baseSelf = NULL;
BaseSpawn* g_baseCompete = NULL;
CCSize g_mapSize;

void TestController::resetAllPal()
{
	char string[30] = { 0 };
	sprintf(string, "%d", m_crntPoint);
	m_lab_point->setText(string);
	if (m_crntPoint >= m_nextLvPoint)
	{
		Vec2 upPos = m_leftPos;
		upPos.y += CARD_UP_DIS;
		moveCard(m_imgLeft, upPos);
		//点亮图片
	}
	else
	{
		moveCard(m_imgLeft, m_leftPos);
	}
	for (int i = 0; i < MAX_NUM; ++i)
	{
		stCard* ac = &m_allCard[i];
		bool canbUse = ac->bCanbeUse;
		if (m_crntPoint > ac->point)
		{
			if (canbUse == false)
			{
				ac->bCanbeUse = true;
				Vec2 upPos = ac->oriPos;
				upPos.y += CARD_UP_DIS;
				moveCard(ac->CardPawn, upPos);
			}
		}
		else if (canbUse == true)
		{
			ac->bCanbeUse = false;
			moveCard(ac->CardPawn, ac->oriPos);
		}
	}
}

void TestController::refreshMainPal(float dt)
{
	if (m_crntPoint < MAX_POINT)
	{
		m_crntPoint += 1;
		resetAllPal();
	}
}

static bool checkDis(Sprite* pS0, Sprite* pS1, float fDis)
{
	bool bRet = false;
	Vec2 vec0 = pS0->getPosition();
	Size s0 = pS0->getContentSize();

	Vec2 vec1 = pS1->getPosition();
	Size s1 = pS1->getContentSize();

	Vec2 divVec = vec0 - vec1;

	float dis = sqrt(pow(divVec.x, 2) + pow(divVec.y, 2));
	return dis <= fDis;
}

bool TestController::detectApproch(vSpawn* detector, vSpawn* eny, Vec2* vecV)
{
	bool bRet = false;
	stBasicInfo detectInfo = detector->m_bInfo;
	float speed = detectInfo.processSpeed;

	if (checkDis(detector->m_view, eny->m_view, detectInfo.detectRadius))
	{
		detector->m_bInfo.bDetected = true;
		detector->m_bInfo.target = (void*)eny;

		Vec2 vecC0 = getCenterPos(detector->m_view);
		Vec2 vecC1 = getCenterPos(eny->m_view);
		*vecV = getFVelocity(vecC0, vecC1, detectInfo.processSpeed);
		bRet = true;
	}
	return bRet;
}

bool TestController::detectAtk(vSpawn* detector, vSpawn* spTarget, Vec2* vecV)
{
	bool bRet = false;
	stBasicInfo detectInfo = detector->m_bInfo;
	float speed = detectInfo.processSpeed;
	Vec2 vecC0 = getCenterPos(detector->m_view);
	vSpawn* pTg = spTarget;
	vSpawn* preTg = (vSpawn*)detector->m_bInfo.target;

	Vec2 vecC1 = getCenterPos(pTg->m_view);
	*vecV = getFVelocity(vecC0, vecC1, speed);

	if (checkDis(detector->m_view, spTarget->m_view, detectInfo.atkRadius))
	{
		if (preTg == NULL)
		{
			detector->m_bInfo.bAtking = true;
			detector->m_bInfo.target = (void*)spTarget;
			fire(getFVelocity(vecC0, getCenterPos(pTg->m_view), detectInfo.bulletSpeed), detector);
			*vecV = Vec2::ZERO;
			bRet = true;
		}
		else if (preTg == spTarget && preTg->isAlive())
		{
			fire(getFVelocity(vecC0, getCenterPos(pTg->m_view), detectInfo.bulletSpeed), detector);
			*vecV = Vec2::ZERO;
			bRet = true;
		}
		else
		{
			bRet = false;
		}
	}
	return bRet;
}

void TestController::traverseArr(std::vector<vSpawn*>& vec1, std::vector<vSpawn*>& vec2)
{
	int nLen1 = vec1.size();
	if (nLen1>0)
	{
		for (int j = 0; j < nLen1; ++j)
		{
			vSpawn* detector = vec1[j];
			if (detector)
			{
				//单个英雄检测所有对象
				int lenOfhero = vec2.size();
				stBasicInfo detectInfo = detector->m_bInfo;
				float speed = detectInfo.processSpeed;
				Vec2 vecV = Vec2(speed, 0);

				Vec2 vecV2 = Vec2(0, 0);
				vSpawn* pBase = (detectInfo.eSpawnType == E_HERO) ? g_baseCompete : g_baseSelf;

				vSpawn* spTarget = (vSpawn*)detector->m_bInfo.target;
				if (detectInfo.bDetected == false || (spTarget && !spTarget->isAlive()))
				{
					if (lenOfhero>0)
					{
						for (int i = 0; i < lenOfhero; ++i)
						{
							if (detectApproch(detector, vec2[i], &vecV))
							{
								detectAtk(detector, vec2[i], &vecV);
								break;
							}
						}
					}

				}
				else if (spTarget && spTarget->isAlive())
				{
					detectAtk(detector, spTarget, &vecV);
				}

				if (detector->m_bInfo.bDetected == false)
				{
					if (detectApproch(detector, pBase, &vecV))
					{
						detectAtk(detector, pBase, &vecV);
					}
				}
				detector->setVelocity(vecV);
				detector->process();
			}
		}

		Vec2 vecV3 = Vec2(0, 0);
		vSpawn* pBase0 = (vec1[0]->m_bInfo.eSpawnType == E_HERO) ? g_baseCompete : g_baseSelf;
		int lenOfeny = vec1.size();
		if (lenOfeny>0)
		{
			for (int i = 0; i < lenOfeny; ++i)
			{
				detectApproch(pBase0, vec1[i], &vecV3);
				if (pBase0->m_bInfo.bDetected == true)
				{
					detectAtk(pBase0, (vSpawn*)(pBase0->m_bInfo.target), &vecV3);
					break;
				}
			}
		}
	}
}

void TestController::checkCollision()
{
	traverseArr(g_vecHeros, g_vecEnemy);
	traverseArr(g_vecEnemy, g_vecHeros);
}

bool ptInRadio(Vec2 center, Vec2 pt, int radius)
{
	double dis = sqrt(pow(pt.x - center.x, 2) + pow(pt.y - center.y, 2));
	if (dis <= radius)
	{
		return true;
	}
	else
	{
		return false;
	}
}

void TestController::recordDelBody(b2Body* bd)
{
	g_mapDestBd.find(bd);
	if (g_mapDestBd.find(bd) != g_mapDestBd.end())
	{
		g_mapDestBd[bd] = g_mapDestBd[bd] + 1;
	}
	else
	{
		g_mapDestBd.insert(std::pair<b2Body*, int>(bd, 1));
	}
}

void TestController::loopBattle(float dt)
{
	g_world->Step(0.1, 8, 1);

	for (std::vector<bulletSpawn*>::iterator iter = g_vecbullet.begin(); iter != g_vecbullet.end();)
	{
		bulletSpawn* blts = (bulletSpawn*)(*iter);
		Vec2 posB = blts->m_view->getPosition();
		if (blts->m_bInfo.bIsAlive == false || posB.x<0 || posB.x>g_mapSize.width || posB.y<0 || posB.y>g_mapSize.height)
		{
			m_mainLayer->removeChild(blts->m_view);
			g_world->DestroyBody(blts->bd);
			iter = g_vecbullet.erase(iter);
		}
		else
		{
			blts->process();
			iter++;
		}
	}

	for (std::vector<vSpawn*>::iterator iter = g_vecHeros.begin(); iter != g_vecHeros.end();)
	{
		vSpawn* heroSpawn = (vSpawn*)(*iter);
		if (!heroSpawn->isAlive())
		{
			m_mainLayer->removeChild(heroSpawn->m_view);
			g_world->DestroyBody(heroSpawn->bd);
			iter = g_vecHeros.erase(iter);
		}
		else
		{
			heroSpawn->process();
			iter++;
		}
	}
	
	for (std::vector<vSpawn*>::iterator iter = g_vecEnemy.begin(); iter != g_vecEnemy.end();)
	{
		vSpawn* enySpawn = (vSpawn*)(*iter);
		if (!enySpawn->isAlive())
		{
			m_mainLayer->removeChild(enySpawn->m_view);
			g_world->DestroyBody(enySpawn->bd);
			iter = g_vecEnemy.erase(iter);
		}
		else
		{
			enySpawn->process();
			iter++;
		}
	}
	checkCollision();

/*	for (b2Body* b = g_world->GetBodyList(); b; b = b->GetNext())
	{
		if (b->GetUserData() != NULL)
		{
			b2Fixture* fixture = b->GetFixtureList();
			if (fixture != NULL)
			{
				uint16 catgory = fixture->GetFilterData().categoryBits;
				if (catgory < 5)
				{
					vSpawn* spawn = (vSpawn*)b->GetUserData();
					b2Vec2 vel = b->GetLinearVelocity();
					stBasicInfo basicInfo = spawn->m_bInfo;
					if (basicInfo.bIsAlive == true)
					{
						b2Fixture* op = (b2Fixture*)(spawn->m_bInfo.target);
						if (!op)
						{
							vel.x = (float)basicInfo.processSpeed / PT_RATIO;
							vel.y = 0;
						}
						else if (g_mapDestBd.find(op->GetBody()) != g_mapDestBd.end())
						{
							spawn->m_bInfo.bDetected = false;
							spawn->m_bInfo.target = (void*)0;
						}
						else
						{
							b2Body* opBody = op->GetBody();
							Vec2 vecPos = getCenterPos(spawn->m_view);
							vSpawn* opSpawn = (vSpawn*)op->GetUserData();

							if (spawn->m_bInfo.bDetected && opSpawn->m_bInfo.bIsAlive)
							{
								Vec2 vecPos2 = getCenterPos(opSpawn->m_view);
								stBasicInfo info = spawn->m_bInfo;
								stBasicInfo infoOp = opSpawn->m_bInfo;

								float speed = info.processSpeed;
								getFVec(&vel, spawn, opSpawn);

								float dis = sqrt(pow(vecPos.x - vecPos2.x, 2) + pow(vecPos.y - vecPos2.y, 2));
								if (dis <= info.atkRadius + info.place)
								{
									Sprite* spBlt = spawn->fire();
									m_mainLayer->addChild(spBlt, spawn->getZOrder());
									vel.x = 0;
									vel.y = 0;
								}
							}
						}

						eSpawnType ePlayer = spawn->m_bInfo.eSpawnType;
						b2Vec2 vecB2 = b->GetPosition();
						if (((ePlayer == E_HERO) && vecB2.x > RIGHT_MOST / PT_RATIO) || ((ePlayer == E_ENEMY) && vecB2.x < LEFT_MOST / PT_RATIO))
						{
							vel.x = 0;
							vel.y = 0;
						}
						b->SetLinearVelocity(vel);
						spawn->process();
					}
					else
					{
						spawn->m_bInfo.bDetected = false;
						spawn->m_bInfo.target = (void*)0;
						recordDelBody(b);
					}
				}
				else
				{
					b2Vec2 vecBullet = b->GetPosition();
					vSpawn* spBullet = (vSpawn*)b->GetUserData();
					if (spBullet->m_bInfo.bIsAlive)
					{
						spBullet->m_view->setPosition(Vec2((vecBullet.x)*PT_RATIO, (vecBullet.y)*PT_RATIO));
					}
					else
					{
						recordDelBody(b);
					}
				}
			}
		}
	}*/
}

void TestController::removeDeadSpawns()
{
	int nLen = g_vecEnemy.size();
	if (nLen)
	{
		for (std::vector<vSpawn*>::iterator iter2 = g_vecEnemy.begin(); iter2 != g_vecEnemy.end();)
		{
			bool bInc = true;
			vSpawn* tmp = *iter2;
			if (tmp->m_bInfo.bIsAlive == false)
			{
				m_mainLayer->removeChild(tmp->m_view);
				iter2 = g_vecEnemy.erase(iter2);
				bInc = false;
			}
			if (bInc)
			{
				iter2++;
			}
		}
	}

	int nLen2 = g_vecHeros.size();
	if (nLen2)
	{
		for (std::vector<vSpawn*>::iterator iter = g_vecHeros.begin(); iter != g_vecHeros.end();)
		{
			bool bInc = true;
			vSpawn* tmp = *iter;
			if (tmp->m_bInfo.bIsAlive == false)
			{
				m_mainLayer->removeChild(tmp->m_view);
				iter = g_vecHeros.erase(iter);
				bInc = false;
			}
			if (bInc)
			{
				iter++;
			}
		}
	}
}

void TestController::moveCard(Node* node, Vec2 PosTo)
{
//	ActionInterval *move = CCMoveTo::create(0.5, pos);
	node->setPosition(PosTo);
}


void TestController::resetCard(int i)
{
	stCard* card = &m_allCard[i];
	Node* node = card->CardPawn;
	Vec2 pos = card->oriPos;
	card->bCanbeUse = false;
	moveCard(node, pos);
	//重置卡牌CD时间
}

void TestController::onDraw()
{
//	glDisable(GL_TEXTURE_2D);
//	glDisableClientState(GL_COLOR_ARRAY);
//	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	g_world->DrawDebugData();

//	glEnable(GL_TEXTURE_2D);
//	glEnableClientState(GL_COLOR_ARRAY);
//	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
}

void TestController::setBasicCardInfo(int i)
{
	stCard temPoint = m_allCard[i];
	ui::Text* lab = temPoint.LabPawn;
	ProgressTimer* progress = temPoint.ProcessPawn;

	char string[30] = { 0 };
	sprintf(string, "cost:%d", temPoint.point);

	lab->setText(string);
//	float percent = temPoint.cdProcess/temPoint.cdPoint;
//	progress->setPercentage(percent);
}

void TestController::loadBasicConfigInfo()
{
	int arrPt[MAX_NUM] = { 30, 22, 40, 38, 50, 48 };

	int arrProcSpeed[MAX_NUM] = { 4, 3, 3, 5, 3, 5 };
	int arrHp[MAX_NUM] = { 80, 60, 140, 40, 30, 40 };

	int arrPlace[MAX_NUM] = { 15, 15, 10, 20, 16, 16 };
	int arrAtkRadius[MAX_NUM] = { 100, 90, 80, 90, 150, 140 };
	int arrDetectRadius[MAX_NUM] = { 400, 400, 400, 400, 400, 400 };

	int arrAtk[MAX_NUM] = { 17, 12, 11, 8, 25, 10 };
	float arrAtkSpan[MAX_NUM] = { 1, 0.83, 0.71, 0.5, 1, 0.41 };
	int arrBulletSpeed[MAX_NUM] = { 17, 14.4, 15.4, 16, 25, 24 };

	for (int i = 0; i < MAX_NUM; ++i)
	{
		m_allCard[i].point = arrPt[i];
		m_allCard[i].cdPoint = 0;
		m_allCard[i].cdProcess = 0;
		m_allCard[i].bCanbeUse = false;

		m_allCard[i].processSpeed = arrProcSpeed[i];
		m_allCard[i].hp = arrHp[i];

		m_allCard[i].place = arrPlace[i];
		m_allCard[i].atkRadius = arrAtkRadius[i];
		m_allCard[i].detectRadius = arrDetectRadius[i];

		m_allCard[i].atk = arrAtk[i];
		m_allCard[i].atkSpan = arrAtkSpan[i];
		m_allCard[i].bulletSpeed = arrBulletSpeed[i];
	}

	g_vecEnemy.clear();
	g_vecHeros.clear();
	g_mapDestBd.clear();
}

void TestController::resetGameParam()
{
	m_crntPoint = 0;
	m_nextLvPoint = g_configEachLv[0];
	m_IncRate = g_configRate[0];
	m_crntLevel = 0;
}

void TestController::addNewSpawn(int type, vSpawn* sp)
{

}

void TestController::addBody4Sprit(vSpawn* pSpawn)
{
	b2BodyDef spriteBodyDef;
	spriteBodyDef.type = b2_dynamicBody;
	Sprite* pSprit = pSpawn->m_view;
	Point ptPosition = pSprit->getPosition();
	
	spriteBodyDef.position = b2Vec2((float)(ptPosition.x / PT_RATIO), (float)(ptPosition.y / PT_RATIO));
	spriteBodyDef.userData = pSpawn;
	b2Body *body = g_world->CreateBody(&spriteBodyDef);
	spriteBodyDef.active = true;
	eSpawnType eIsPlayer = pSpawn->m_bInfo.eSpawnType;

	b2CircleShape circle;
	circle.m_radius = pSpawn->m_bInfo.place / PT_RATIO;
	b2FixtureDef place;
	place.isSensor = false;
	place.shape = &circle;
	place.density = 0.0f;
	place.restitution = 0.0f;
	place.friction = 0.0f;
	place.userData = pSpawn;
	place.filter.categoryBits = (eIsPlayer == E_HERO || eIsPlayer == E_BASE_HERO) ? 0x0001 : 0x0002;
	place.filter.maskBits = (eIsPlayer == E_HERO || eIsPlayer == E_BASE_HERO) ? 0x0005 : 0x0006;
	body->CreateFixture(&place);  
	pSpawn->bd = body;

	body->SetLinearVelocity(b2Vec2(pSpawn->m_bInfo.processSpeed / PT_RATIO, 0));
}

//spawn 新的英雄
void TestController::spawnNewRole(int nCardIdx)
{
	char string[30] = { 0 };
	sprintf(string, "role%d.png", nCardIdx);
	vSpawn* hero0 = new vSpawn;
	stCard cardSt = m_allCard[nCardIdx];
	stBasicInfo mConfig = { nCardIdx, cardSt.processSpeed, 100, cardSt.atk, cardSt.atkSpan, 0.0, cardSt.bulletSpeed, cardSt.hp,	g_vecHeros.size(),
							(void*)0, cardSt.place, cardSt.atkRadius, cardSt.detectRadius, true, true, E_HERO, false, false, b2Vec2(0, 0) };
	hero0->create(string, mConfig);
	Sprite* sprite = hero0->m_view;
	m_mainLayer->addChild(sprite, 5 - nCardIdx);
	int disY = nCardIdx * SPAN;
	Point ptPosition = Point(LEFT_MOST, disY);
	sprite->setPosition(ptPosition);
	g_vecHeros.push_back(hero0);
	addBody4Sprit((vSpawn*)g_vecHeros[g_vecHeros.size()-1]);
}

//spawn 新敌人
void TestController::spawnNewEnemy(int nIdx)
{
	int arrPt[MAX_NUM] = { 30, 22, 40, 38, 50, 48 };

	int arrProcSpeed[MAX_NUM] = { -4, -3, -3, -5, -3, -5 };
	int arrHp[MAX_NUM] = { 80, 60, 140, 40, 30, 40 };

	int arrPlace[MAX_NUM] = { 15, 13, 18, 20, 20, 20 };
	int arrAtkRadius[MAX_NUM] = { 100, 90, 80, 90, 150, 140 };
	int arrDetectRadius[MAX_NUM] = { 400, 400, 400, 400, 400, 400 };

	int arrAtk[MAX_NUM] = { 17, 12, 11, 8, 25, 10 };
	float arrAtkSpan[MAX_NUM] = { 1, 0.83, 0.71, 0.5, 1, 0.41 };
	int arrBulletSpeed[MAX_NUM] = { 17, 14.4, 15.4, 16, 25, 24 };

	g_channel += 1;
	if (g_channel > 5)
	{
		g_channel -= 6;
	}

	char string[30] = { 0 };
	sprintf(string, "role%d.png", nIdx);
	vSpawn* eni = new vSpawn;
	stBasicInfo mConfig = { 0, arrProcSpeed[nIdx], 100, arrAtk[nIdx], arrAtkSpan[nIdx], 0.0, arrBulletSpeed[nIdx], arrHp[nIdx],
		g_vecEnemy.size(), (void*)0, arrPlace[nIdx], arrAtkRadius[nIdx], arrDetectRadius[nIdx], true, true, E_ENEMY, false, false, b2Vec2(0, 0) };
	eni->create(string, mConfig);
	Sprite* sprite = eni->m_view;
	sprite->setColor(Color3B::GREEN);
	int disY = g_channel * SPAN;
	Vec2 pos = Vec2(RIGHT_MOST, disY);
	sprite->setPosition(pos);
	m_mainLayer->addChild(sprite, g_channel * 2);
	g_vecEnemy.push_back(eni);

	addBody4Sprit((vSpawn*)eni);
}

bool TestController::CardListener(Touch* touch, Event* event)
{
	return false;
}


void TestController::initTileMap()
{

}

void TestController::setUpPhysicalWord()
{
	b2Vec2 gravity(0, 0);
	g_world = new b2World(gravity);
	g_world->SetAllowSleeping(true);
	g_world->SetContinuousPhysics(true);

	contactListener = new MyContactListener();
	g_world->SetContactListener((b2ContactListener*)contactListener);

	m_debugDraw = new (std::nothrow) GLESDebugDraw(PT_RATIO);
	g_world->SetDebugDraw(m_debugDraw); //注册到Box2d的world对象里面  
	uint32 flags = 0;
	flags += b2Draw::e_shapeBit; //要绘制的信息，这里只绘制Box2d的Shape。  
	m_debugDraw->SetFlags(flags);
	onDraw();
}


void TestController::setUpBase()
{
	stBasicInfo baseConfig = { 0, 0, 100, 10, 1, 0.0, 26, 1000, 1, (void*)0, 80.0, 300.0, 300.0, true, true, E_BASE_HERO, false, false, b2Vec2(0, 0) };
	float fH = m_mainLayer->getContentSize().height*0.5;
	g_baseSelf = new BaseSpawn;
	g_baseSelf->create("castle.png", baseConfig);
	addBody4Sprit((vSpawn*)g_baseSelf);
	g_baseSelf->m_view->setPosition(Vec2(LEFT_MOST - 50, fH));
	m_mainLayer->addChild(g_baseSelf->m_view, 2000);

	baseConfig.gId = 1;
	baseConfig.eSpawnType = E_BASE_ENEMY;
	g_baseCompete = new BaseSpawn;
	g_baseCompete->create("castle.png", baseConfig);
	addBody4Sprit((vSpawn*)g_baseCompete);
	g_baseCompete->m_view->setPosition(Vec2(RIGHT_MOST + 50, fH));
	m_mainLayer->addChild(g_baseCompete->m_view, 2000);
}

CCNode* TestController::setUpUI()
{
	auto mainLayer = cocostudio::GUIReader::getInstance()->widgetFromJsonFile("NewUi_1.json");
	this->addChild(mainLayer, 1);

	static EventListenerTouchOneByOne* listener0 = EventListenerTouchOneByOne::create();
	listener0->setSwallowTouches(true);
	listener0->onTouchBegan = [=](Touch* touch, Event* event){
		//		auto target = event->getCurrentTarget();
		_beginPos = touch->getLocation();
		_tgPos = m_mainLayer->getPosition();
		return true;
	};
	listener0->onTouchMoved = [=](Touch* touch, Event* event){
		Vec2 pos = touch->getLocation();
		int disX = pos.x - _beginPos.x + _tgPos.x;
		if (disX <= 0 && disX >= -500)
		{
			m_mainLayer->setPosition(Vec2(disX, 100));
		}
	};
	listener0->onTouchEnded = [=](Touch* touch, Event* event){
		auto target = event->getCurrentTarget();
		_beginPos = target->getPosition();
	};
	listener0->onTouchCancelled = [=](Touch* touch, Event* event){
		auto target = event->getCurrentTarget();
		_beginPos = target->getPosition();
	};
	_eventDispatcher->addEventListenerWithSceneGraphPriority(listener0, m_mainLayer);
	mainLayer->retain();
	mainLayer:setTouchEnabled(true);

	static auto palUI = mainLayer->getChildByName("palUI");

	m_imgLeft = palUI->getChildByName("img_left");
	m_leftPos = m_imgLeft->getPosition();
	m_labNextPoint = (ui::Text*)(m_imgLeft->getChildByName("lab_nextPoint"));
	m_lab_point = (ui::Text*)(m_imgLeft->getChildByName("lab_point"));
	m_labLv = (ui::Text*)m_imgLeft->getChildByName("lab_lv");
	char string[30] = { 0 };

	sprintf(string, "%d", m_crntPoint);
	m_lab_point->setText(string);

	sprintf(string, "%d", m_crntLevel + 1);
	m_labLv->setText(string);

	sprintf(string, "%d", g_configEachLv[m_crntLevel]);
	m_labNextPoint->setText(string);

	static EventListenerTouchOneByOne* listener = EventListenerTouchOneByOne::create();
	listener->setSwallowTouches(true);
	listener->onTouchBegan = [=](Touch* touch, Event* event){
		auto target = static_cast<Node*>(event->getCurrentTarget());
		Vec2 posPre = touch->getLocation();
		Vec2 locationInNode = target->convertToNodeSpace(touch->getLocation());
		Size s = target->getContentSize();

		bool bRect = false;
		int nIdx = 0;
		stCard* tmp = &(m_allCard[0]);
		for (int i = 0; i < MAX_NUM; ++i)
		{
			ui::ImageView* imgV = m_allCard[0].CardPawn;
			Size tsize = imgV->getContentSize();
			Vec2 nowPos = imgV->getPosition();
			int w = tsize.width;
			int h = tsize.height;
			Rect tmpRect = Rect(w*i, 0, w, h);
			if (tmpRect.containsPoint(locationInNode))
			{
				bRect = true;
				nIdx = i;
				break;
			}
		}

		if (bRect)
		{
			stCard* tmp = &(m_allCard[nIdx]);
			if (tmp->bCanbeUse == true)
			{
				if (m_crntPoint < tmp->point)
				{
					moveCard(target, tmp->oriPos);

					//重置卡牌CD时间
					//ZeroMemory(tmp, sizeof(stCard));
				}
				m_crntPoint -= tmp->point;

				//spawn 出新的英雄
				spawnNewRole(tmp->nCardIdx);

				resetAllPal();
			}
			resetAllPal();
		}
		return false;
	};

	static EventListenerTouchOneByOne* listenerLeft = EventListenerTouchOneByOne::create();
	listenerLeft->setSwallowTouches(true);
	listenerLeft->onTouchBegan = [=](Touch* touch, Event* event){
		auto target = static_cast<Sprite*>(event->getCurrentTarget());
		m_nextLvPoint = g_configEachLv[m_crntLevel];
		Vec2 touchPos = target->convertToNodeSpace(touch->getLocation());
		for (int i = 0; i < MAX_NUM; ++i)
		{
			Size tsize = m_imgLeft->getContentSize();
			Vec2 nowPos = target->getPosition();
			int w = tsize.width;
			int h = tsize.height;
			Rect tmpRect = Rect(0, 0, w, h);
			if (tmpRect.containsPoint(touchPos))
			{
				if (m_crntPoint >= m_nextLvPoint && m_crntLevel < MAX_LEVEL)
				{
					m_crntPoint -= g_configEachLv[m_crntLevel];
					m_crntLevel += 1;
					m_IncRate = g_configRate[m_crntLevel];

					char string[30] = { 0 };
					sprintf(string, "%d", m_crntLevel + 1);
					m_labLv->setText(string);

					sprintf(string, "%d", g_configEachLv[m_crntLevel]);
					m_labNextPoint->setText(string);
					m_nextLvPoint = g_configEachLv[m_crntLevel];

					resetAllPal();
					if (m_mainLayer != NULL)
					{
						m_mainLayer->unscheduleAllSelectors();
						m_mainLayer->schedule(SEL_SCHEDULE(&TestController::refreshMainPal), m_IncRate);
					}
				}
			}
		}
		return false;
	};

	for (int i = 0; i < (int)MAX_NUM; ++i)
	{
		//缓存Card
		std::ostringstream ostr;
		ostr << "img" << i;
		std::string temStr = ostr.str();

		ui::ImageView* imgPal = (ui::ImageView*)(palUI->getChildByName(temStr));
		ui::Text* lab = (ui::Text*)imgPal->getChildByName("lab");
		ProgressTimer* progress = (ProgressTimer*)(palUI->getChildByName("progress"));
		ui::ImageView* img = (ui::ImageView*)(palUI->getChildByName("img"));

		stCard* tmpCard = &m_allCard[i];
		tmpCard->LabPawn = lab;
		tmpCard->ProcessPawn = progress;
		tmpCard->CardPawn = imgPal;

		//设置Card信息
		setBasicCardInfo(i);

		tmpCard->oriPos.x = imgPal->getPosition().x;
		tmpCard->oriPos.y = imgPal->getPosition().y;
		tmpCard->nCardIdx = i;
		//		imgPal->setUserData(&m_allCard[i]);
	}
	_eventDispatcher->addEventListenerWithSceneGraphPriority(listener, m_allCard[0].CardPawn);
	_eventDispatcher->addEventListenerWithSceneGraphPriority(listenerLeft, m_imgLeft);

	static auto palUI2 = mainLayer->getChildByName("palUI2");
	m_btnEnemy = (Node*)(palUI2->getChildByName("lab_eny_5"));

	static EventListenerTouchOneByOne* listener1 = EventListenerTouchOneByOne::create();
	listener1->setSwallowTouches(true);
	listener1->onTouchBegan = [=](Touch* touch, Event* event){
		auto target = static_cast<Node*>(event->getCurrentTarget());
		Vec2 posPre = touch->getLocation();
		Vec2 locationInNode = target->convertToNodeSpace(touch->getLocation());
		Size s = target->getContentSize();

		bool bRect = false;
		int nIdx = 0;
		for (int i = 0; i < MAX_NUM; ++i)
		{
			Size tsize = m_btnEnemy->getContentSize();
			Vec2 nowPos = m_btnEnemy->getPosition();
			int w = tsize.width;
			int h = 30;
			Rect tmpRect = Rect(0, h*(5-i), w, h);
			if (tmpRect.containsPoint(locationInNode))
			{
				bRect = true;
				nIdx = i;
				break;
			}
		}

		if (bRect)
		{
			spawnNewEnemy(nIdx);
		}
		return false;
	};
	_eventDispatcher->addEventListenerWithSceneGraphPriority(listener1, m_btnEnemy);

	return mainLayer;
}

bool TestController::initMainLayer()
{
	CCTMXTiledMap *tMap = CCTMXTiledMap::create("mapBattle.tmx");
	tMap->retain();
	tMap->setPosition(Vec2(0, 100));
	this->addChild(tMap, 0);
	//	CCArray * pChildrenArray = tMap->getChildren();
	m_mainLayer = (ui::Widget*)tMap;
	g_mapSize = m_mainLayer->getContentSize();
	return true;
}


TestController::TestController()
: _beginPos(Vec2::ZERO)
{
	resetGameParam();
	loadBasicConfigInfo();
	setUpPhysicalWord();
	initMainLayer();

	setUpBase();
	setUpUI();
}

ui::Widget* TestController::getMainLayer()
{
	return m_mainLayer;
}


TestController::~TestController()
{

}


void TestController::startAutoRun()
{
	if (m_mainLayer != NULL)
	{
		m_mainLayer->unscheduleAllSelectors();
		Scheduler* scheduler = Director::getInstance()->getScheduler();
		m_mainLayer->schedule(SEL_SCHEDULE(&TestController::refreshMainPal), m_IncRate);
		scheduler->schedule(CC_SCHEDULE_SELECTOR(TestController::loopBattle), this, TIME_TICK, false);
	}
}

ssize_t TestController::readline(int fd, char* ptr, size_t maxlen)
{
    size_t n, rc;
    char c;

    for( n = 0; n < maxlen - 1; n++ ) {
        if( (rc = recv(fd, &c, 1, 0)) ==1 ) {
            *ptr++ = c;
            if(c == '\n') {
                break;
            }
        } else if( rc == 0 ) {
            return 0;
        } else if( errno == EINTR ) {
            continue;
        } else {
            return -1;
        }
    }

    *ptr = 0;
    return n;
}

BattleLayer::~BattleLayer()
{

}

BattleLayer::BattleLayer() 
: _beginPos(Vec2::ZERO)
{
	auto mainLayer = cocostudio::GUIReader::getInstance()->widgetFromJsonFile("NewUi_2.json");
	this->addChild(mainLayer);
	mainLayer->retain();
	m_mainLayerBattle = mainLayer;
	mainLayer:setTouchEnabled(true);

}


bool BattleLayer::onTouchBegan(Touch* touch, Event  *event)
{
	_beginPos = touch->getLocation();
	return true;
}

void BattleLayer::onTouchMoved(Touch* touch, Event  *event)
{
	auto touchLocation = touch->getLocation();

//	s_tCurPos = touchLocation;
}

void BattleLayer::onMouseScroll(Event *event)
{
}

bulletSpawn::bulletSpawn()
{

}

bulletSpawn::~bulletSpawn()
{

}



Sprite* bulletSpawn::create(const char* filename, stBasicInfo config)
{
	Sprite* sprite = Sprite::create(filename);
	if (sprite)
	{
		sprite->retain();
		m_view = sprite;
	}
	else
	{
		//		CC_SAFE_DELETE(sprite);
	}
	m_bInfo = config;

	return sprite;
}

bool bulletSpawn::playHit(int lost)
{
	return false;
}

void bulletSpawn::process()
{
	if (m_view != NULL)
	{
		b2Vec2 pos = bd->GetPosition();
		Vec2 vecNow = Vec2(pos.x*PT_RATIO, pos.y*PT_RATIO);
		m_view->setPosition(vecNow);
	}
}

vSpawn::~vSpawn()
{

}

vSpawn::vSpawn()
{

}

bool TestController::fire(Vec2 vecPath, vSpawn* vsp)
{
	vsp->m_bInfo.atkTick += TIME_TICK;
	if (vsp->m_bInfo.atkTick >= vsp->m_bInfo.atkSpan)
	{
		vsp->m_bInfo.atkTick = 0;
		bulletSpawn* bullet = new bulletSpawn;
		Point ptPosition = getCenterPos(vsp->m_view);

		stBasicInfo stInfo = vsp->m_bInfo;
		bullet->m_bInfo.eSpawnType = E_BULLET;
		bullet->create("bullet.png", stInfo);

		Sprite* sprite = bullet->m_view;
		sprite->setPosition(ptPosition);
		bullet->m_bInfo.target = (void*)vsp;

		g_vecbullet.push_back(bullet);

		b2BodyDef bulletBodyDef;
		bulletBodyDef.type = b2_dynamicBody;
		bulletBodyDef.bullet = true;

		bulletBodyDef.position = b2Vec2((float)(ptPosition.x / PT_RATIO), (float)(ptPosition.y / PT_RATIO));
		bulletBodyDef.userData = (void*)bullet;
		b2Body *body = g_world->CreateBody(&bulletBodyDef);
		bulletBodyDef.active = true;
		eSpawnType eIsPlayer = stInfo.eSpawnType;

		b2CircleShape circle2;
		circle2.m_radius = 10.0 / PT_RATIO;
		b2FixtureDef range;
		range.isSensor = true;
		range.shape = &circle2;
		range.density = 0.0f;
		range.restitution = 0.0f;
		range.userData = (void*)bullet;
		range.filter.categoryBits = (eIsPlayer == E_HERO || eIsPlayer == E_BASE_HERO) ? 0x0005 : 0x0006;
		range.filter.maskBits = (eIsPlayer == E_HERO || eIsPlayer == E_BASE_HERO) ? 0x0002 : 0x0001;
		body->CreateFixture(&range);

		float speed = stInfo.bulletSpeed;
		bullet->bd = body;
		body->SetLinearVelocity(b2Vec2(vecPath.x *speed / PT_RATIO, vecPath.y*speed / PT_RATIO));
		m_mainLayer->addChild(bullet->m_view);
	}

	return true;
}

Sprite* vSpawn::create(const char* filename, stBasicInfo config)
{
	Sprite* sprite = Sprite::create(filename);
	if (sprite)
	{
		sprite->retain();
		m_view = sprite;
	}
	else
	{
//		CC_SAFE_DELETE(sprite);
	}
	m_bInfo = config;

	return sprite;
}

bool vSpawn::isAlive()
{
	bool ret = false;
	if (m_bInfo.hp > 0)
	{
		ret = true;
	}
	return ret;
}

void vSpawn::setVelocity(Vec2 vec)
{
	m_bInfo.detecVec.x = vec.x / PT_RATIO;
	m_bInfo.detecVec.y = vec.y / PT_RATIO;
	bd->SetLinearVelocity(m_bInfo.detecVec);
}

bool vSpawn::addHp(int num)
{
	//记得添加maxHp
	m_bInfo.hp += num;

	return true;
}

bool vSpawn::playHit(int lost)
{
	if (m_bInfo.hp <= 0)
	{
		m_bInfo.bIsAlive = false;
		return false;
	}

	Vec2 pos = m_view->getPosition();
	CCSize size = m_view->getContentSize();
	char string[30] = { 0 };
	sprintf(string, "-%d", lost);
	CCLabelTTF* label = CCLabelTTF::create(string, "Arial", 20);
	label->setColor(Color3B::RED);
	m_view->addChild(label, 100);
	label->setPosition(Vec2(size.width*0.5, size.height*0.5));

//	CCActionInterval* scaleAction1 = CCMoveBy::create(0.5f, ccp(pos.x + size.width*0.5, pos.y + size.height + 10));

//	CCActionInterval* scaleAction2 = CCFadeOut::create(0.3f);

	CCAction *action = CCSequence::create(
		CCMoveBy::create(1.0f, ccp(0, size.height)),
		CCFadeOut::create(0.2f),
		NULL
		);
	label->runAction(action);
	m_bInfo.hp -= lost;
	if (m_bInfo.hp <= 0)
	{
		RemoveSelf* action = RemoveSelf::create(true);
		this->runAction(action);
		m_bInfo.bIsAlive = false;
		return false;
	}
	else
	{
		return true;
	}
}

void vSpawn::process()
{
	if (m_view != NULL)
	{
		b2Vec2 pos = bd->GetPosition();
		Vec2 vecNow = Vec2(pos.x*PT_RATIO, pos.y*PT_RATIO);
		if (vecNow.x <= RIGHT_MOST && vecNow.x >= LEFT_MOST)
		{
			m_view->setPosition(vecNow);
		}
	}
}


enemy::~enemy()
{

}

enemy::enemy()
{

}

hero::~hero()
{

}

hero::hero()
{

}

BaseSpawn::~BaseSpawn()
{

}

BaseSpawn::BaseSpawn()
{

}

