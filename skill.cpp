#include "skill.h"
#include "tilemap.h"
#include <QGraphicsScene>
#include <QBrush>
#include <QPen>
#include <QDebug>

// ============================================================================
//  Projectile 流星粒子（一技能）
// ============================================================================

Projectile::Projectile(QPointF startPos, QPointF velocity, int damage,
                       bool decay, TileMap *tileMap, QGraphicsScene *scene,
                       QGraphicsItem *parent)
    : QGraphicsEllipseItem(parent),
      velocity(velocity),
      damage(damage),
      decay(decay),
      tileMap(tileMap),
      m_scene(scene),
      lifetime(decay ? 60 : -1),
      initialRadius(6.0)
{
    // 设置流星粒子的形状：圆形
    setRect(-initialRadius, -initialRadius, initialRadius * 2, initialRadius * 2);
    setPos(startPos);

    // 流星颜色：金色填充 + 红橙色发光边框
    setBrush(QBrush(QColor(255, 215, 0)));
    setPen(QPen(QColor(255, 100, 0), 2));

    // 自动加入场景
    if (scene) {
        scene->addItem(this);
    }
}

Projectile::~Projectile()
{
    // QGraphicsItem 的析构会自动从场景中移除自己，无需手动 removeItem
}

bool Projectile::update()
{
    // ========== 1. 移动 ==========
    setPos(pos() + velocity);

    // ========== 2. 墙壁碰撞检测 ==========
    if (tileMap && tileMap->collidesWithWall(this)) {
        return false; // 撞墙 -> 死亡
    }

    // ========== 3. 场景边界检测 ==========
    if (m_scene) {
        QRectF sceneRect = m_scene->sceneRect();
        if (!sceneRect.contains(pos())) {
            return false; // 飞出场景 -> 死亡
        }
    }

    // ========== 4. 衰减处理 ==========
    if (decay && lifetime > 0) {
        lifetime--;
        if (lifetime <= 0) {
            return false; // 寿命耗尽 -> 死亡
        }

        // 半径逐渐缩小
        qreal ratio = static_cast<qreal>(lifetime) / 60.0;
        qreal r = initialRadius * ratio;
        if (r < 0.5) {
            r = 0.5;
        }
        setRect(-r, -r, r * 2, r * 2);

        // 颜色渐变：金色 -> 暗橙 -> 接近透明
        int red   = 255;
        int green = static_cast<int>(180 * ratio);
        int blue  = static_cast<int>(50 * ratio);
        int alpha = static_cast<qreal>(200 * ratio + 55);

        setBrush(QBrush(QColor(red, green, blue, alpha)));
        setPen(QPen(QColor(255, 80, 0, alpha), 2));
    }

    return true; // 仍然存活
}

// ============================================================================
//  BladeWave 刀浪（二技能）
// ============================================================================

BladeWave::BladeWave(QPointF startPos, QPointF velocity, int damage,
                     TileMap *tileMap, QGraphicsScene *scene,
                     QGraphicsItem *parent)
    : QGraphicsRectItem(parent),
      velocity(velocity),
      damage(damage),
      tileMap(tileMap),
      m_scene(scene),
      maxDistance(400),
      distanceTraveled(0.0)
{
    // 刀浪形状：根据飞行方向设置长条形矩形（长度 80，宽度 16）
    // 矩形从 startPos 向飞行方向延伸
    qreal vx = velocity.x();
    qreal vy = velocity.y();
    if (qAbs(vx) >= qAbs(vy)) {
        // 水平方向为主
        if (vx >= 0) {
            setRect(0, -8, 80, 16);    // 向右：从原点向右延伸
        } else {
            setRect(-80, -8, 80, 16);  // 向左：从原点向左延伸
        }
    } else {
        // 垂直方向为主
        if (vy >= 0) {
            setRect(-8, 0, 16, 80);    // 向下：从原点向下延伸
        } else {
            setRect(-8, -80, 16, 80);  // 向上：从原点向上延伸
        }
    }
    setPos(startPos);

    // 刀浪颜色：青蓝色填充 + 银白色发光边框（炫酷感）
    setBrush(QBrush(QColor(0, 200, 255, 200)));
    setPen(QPen(QColor(200, 240, 255, 230), 2));

    // 自动加入场景
    if (scene) {
        scene->addItem(this);
    }
}

BladeWave::~BladeWave()
{
    // QGraphicsItem 的析构会自动从场景中移除自己
}

bool BladeWave::update()
{
    // ========== 1. 移动 ==========
    QPointF oldPos = pos();
    setPos(oldPos + velocity);

    // 累计飞行距离
    distanceTraveled += QLineF(oldPos, pos()).length();
    if (distanceTraveled >= maxDistance) {
        return false; // 达到最大飞行距离 -> 死亡
    }

    // ========== 2. 墙壁碰撞检测 ==========
    if (tileMap && tileMap->collidesWithWall(this)) {
        return false; // 撞墙 -> 死亡
    }

    // ========== 3. 场景边界检测 ==========
    if (m_scene) {
        QRectF sceneRect = m_scene->sceneRect();
        if (!sceneRect.contains(pos())) {
            return false; // 飞出场景 -> 死亡
        }
    }

    return true; // 仍然存活
}
