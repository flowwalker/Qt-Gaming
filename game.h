#ifndef GAME_H
#define GAME_H

#include <QGraphicsView>
#include <QTimer>
#include <QKeyEvent>
#include <QString>
#include <QVector>
#include <QList>
#include "maploader.h"   // 必须包含，因为使用了 Portal 结构体
#include "skill.h"
#include "enemy.h"

class Player;
class TileMap;
class Projectile;
class SimpleProjectile;
class Enemy;
class EnemyProjectile;
class Spawner;
class Pet;
class QMovie;

class Game : public QGraphicsView
{
    Q_OBJECT
public:
    Game(QWidget *parent = nullptr);
    ~Game();

    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void loadMap(const QString &mapFilePath, bool useStartPoint = true);

    /** 敌人注册的炮弹加入游戏管理列表 */
    void addEnemyProjectile(EnemyProjectile *ep);
    /** 巢穴生成的敌人加入游戏管理列表 */
    void addEnemy(Enemy *e);
    /** 获取当前场上敌人数量 */
    int getEnemyCount() const { return enemies.size(); }

private slots:
    void updateGame();
    void checkPortal();
    void performTeleport(const Portal &portal); // 现在 Portal 已定义

private:
    QGraphicsScene *scene = nullptr;
    Player *player = nullptr;
    TileMap *tileMap = nullptr;
    QTimer *gameTimer = nullptr;

    bool upPressed, downPressed, leftPressed, rightPressed;
    QString currentMapPath;
    bool canTeleport;
    bool isTeleporting;

    int regenCounter = 0; // HP/MP 恢复计时器（每 60 帧恢复 1 点）

    // 视野缩放
    qreal zoomLevel = 1.0;
    static constexpr qreal ZOOM_STEP = 1.15;
    static constexpr qreal MIN_ZOOM = 0.3;
    static constexpr qreal MAX_ZOOM = 4.0;
    void applyZoom();

    // 闪现动画状态
    struct FlashState {
        bool active = false;
        QPointF step;       // 每帧移动量
        int framesLeft = 0; // 剩余帧数
        QPointF finalPos;   // 最终目标位置
        QPointF bladeDir;   // 刀浪方向（闪现结束时发射）
    };
    FlashState flashState;

    // ========== HUD 血蓝条 ==========
    QGraphicsRectItem *hudHpBg = nullptr;
    QGraphicsRectItem *hudHpFg = nullptr;
    QGraphicsRectItem *hudMpBg = nullptr;
    QGraphicsRectItem *hudMpFg = nullptr;
    QGraphicsRectItem *hudExpBg = nullptr;
    QGraphicsRectItem *hudExpFg = nullptr;
    QGraphicsSimpleTextItem *hudText = nullptr;
    QGraphicsSimpleTextItem *hudLevelText = nullptr;

    void createHud();
    void updateHud();

    // ========== 经验等级系统 ==========
    void onPlayerLevelUp(int newLevel);
    void applyLevel10Enhancement();
    void playTransformAnimation(); // 3级变身动画

    // ========== 宠物系统 ==========
    Pet *pet = nullptr;

    // ========== 技能系统 ==========
    /** 当前场景中所有活跃的流星粒子 */
    QVector<Projectile*> projectiles;
    /** 当前场景中所有活跃的简易子弹（1-2级红色椭圆） */
    QVector<SimpleProjectile*> simpleProjectiles;
    /** 当前场景中所有活跃的刀浪 */
    QVector<BladeWave*> bladeWaves;
    /** 可被攻击的对象列表（Boss、小怪等），供碰撞检测使用 */
    QList<QGraphicsItem*> hittableItems;

    bool gamePaused = false;        // 变身动画期间暂停游戏
    bool explosionsEnabled = false; // 5级后启用爆炸效果
    bool fireBgEnabled = false;     // 5级后启用背后火焰
    QGraphicsPixmapItem *transformItem = nullptr;
    QMovie *transformMovie = nullptr;
    QGraphicsPixmapItem *fireBgItem = nullptr;
    int fireBgFrameIdx = 0;
    int fireBgTick = 0;

    /** 技能一：粒子爆发（按 I 键触发） */
    void skillMeteorBurst();
    /** 每帧更新所有流星粒子（移动、碰撞、清理） */
    void updateProjectiles();
    /** 每帧更新所有简易子弹（移动、碰撞、清理） */
    void updateSimpleProjectiles();
    /** 在指定位置创建爆炸动画（3x3 tile 大小） */
    void createExplosion(QPointF centerPos);

    /** 技能二：闪现刀浪（按 K 键触发） */
    void skillFlashBlade();
    /** 每帧更新所有刀浪（移动、碰撞、清理） */
    void updateBladeWaves();

    /** 普攻：九宫格火光（按 J 键触发） */
    void skillNormalAttack();

    /** 技能三：盾牌（按住 L 键激活，松开关闭） */
    void skillShieldActivate();
    void skillShieldDeactivate();
    void updateShieldPosition(); // 每帧跟随玩家
    QGraphicsEllipseItem *shieldItem = nullptr; // 盾牌图形项
    bool shieldActive = false;                  // 盾牌是否激活

    /** 根据当前按键状态获取闪现/刀浪方向向量 */
    QPointF getCurrentDirectionVector();

private:
    // ========== 敌人系统 ==========
    QVector<Enemy*> enemies;                // 所有活跃敌人
    QVector<EnemyProjectile*> enemyProjectiles; // 所有活跃敌人炮弹

    /** 每帧更新所有敌人（移动、攻击） */
    void updateEnemies();
    /** 每帧更新所有敌人炮弹（移动、碰撞、清理） */
    void updateEnemyProjectiles();

    // ========== 巢穴系统 ==========
    QVector<Spawner*> spawners;             // 所有活跃巢穴

    /** 每帧更新所有巢穴 */
    void updateSpawners();
};

#endif // GAME_H