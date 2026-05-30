#ifndef SKILL_H
#define SKILL_H

#include <QGraphicsEllipseItem>
#include <QGraphicsRectItem>
#include <QObject>
#include <QPointF>

class QGraphicsScene;
class TileMap;

/**
 * @brief Projectile 流星粒子类
 *
 * 表示从玩家发出的一个流星粒子投射物。
 * 每帧自动更新位置，检测墙壁碰撞和场景边界，支持衰减效果。
 */
class Projectile : public QObject, public QGraphicsEllipseItem
{
    Q_OBJECT
public:
    /**
     * @brief 构造函数
     * @param startPos   起始位置（场景坐标）
     * @param velocity   速度向量（每帧移动的像素值）
     * @param damage     伤害值（预留，供后续血量系统使用）
     * @param decay      是否衰减（true=逐渐变小消失，false=恒速飞行直到撞墙）
     * @param tileMap    地图指针，用于墙壁碰撞检测
     * @param scene      场景指针，用于边界检测和自动加入场景
     * @param parent     父图形项
     */
    Projectile(QPointF startPos, QPointF velocity, int damage,
               bool decay, TileMap *tileMap, QGraphicsScene *scene,
               QGraphicsItem *parent = nullptr);
    ~Projectile();

    /**
     * @brief 每帧更新粒子状态
     * @return true  粒子仍然存活，继续存在
     * @return false 粒子已死亡（撞墙/出界/衰减完毕），需要被销毁
     */
    bool update();

    /** @return 伤害值 */
    int getDamage() const { return damage; }

private:
    QPointF velocity;      // 速度向量（像素/帧）
    int damage;            // 伤害值（预留接口）
    bool decay;            // 是否启用衰减
    TileMap *tileMap;      // 地图指针（墙壁碰撞检测）
    QGraphicsScene *m_scene; // 场景指针（边界检测）
    int lifetime;          // 剩余生命周期（帧数），-1 表示不衰减/无限
    qreal initialRadius;   // 初始半径（像素）
};

// ============================================================================
//  二技能：刀浪（Blade Wave）
// ============================================================================

/**
 * @brief BladeWave 刀浪类
 *
 * 闪现后射出的长条形刀气，向指定方向飞行。
 * 撞到墙壁或击中敌人后消失。
 */
class BladeWave : public QObject, public QGraphicsRectItem
{
    Q_OBJECT
public:
    /**
     * @brief 构造函数
     * @param startPos   起始位置（场景坐标，通常是玩家闪现后的前方）
     * @param velocity   速度向量（每帧移动的像素值）
     * @param damage     伤害值
     * @param tileMap    地图指针，用于墙壁碰撞检测
     * @param scene      场景指针
     * @param parent     父图形项
     */
    BladeWave(QPointF startPos, QPointF velocity, int damage,
              TileMap *tileMap, QGraphicsScene *scene,
              QGraphicsItem *parent = nullptr);
    ~BladeWave();

    /**
     * @brief 每帧更新刀浪状态
     * @return true  刀浪仍然存活
     * @return false 刀浪已死亡（撞墙/出界/击中敌人），需要被销毁
     */
    bool update();

    /** @return 伤害值 */
    int getDamage() const { return damage; }

private:
    QPointF velocity;       // 速度向量（像素/帧）
    int damage;             // 伤害值
    TileMap *tileMap;       // 地图指针（墙壁碰撞检测）
    QGraphicsScene *m_scene; // 场景指针（边界检测）
    int maxDistance;        // 最大飞行距离（像素）
    qreal distanceTraveled; // 已飞行距离（像素）
};

#endif // SKILL_H
