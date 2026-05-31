#ifndef PLAYER_H
#define PLAYER_H

#include <QGraphicsPixmapItem>
#include <QObject>
#include <QMovie>

class TileMap;

class Player : public QObject, public QGraphicsPixmapItem
{
    Q_OBJECT
public:
    explicit Player(TileMap *map, QGraphicsItem *parent = nullptr);
    ~Player();

    void move(bool up, bool down, bool left, bool right);
    /** 碰撞判定框：显示 64×64，但只取右下角 32×32 做墙碰撞 */
    QRectF hitboxRect() const { return QRectF(x() + 32, y() + 32, 32, 32); }

    // ========== 血量蓝量系统 ==========
    int getHp() const { return hp; }
    int getMaxHp() const { return maxHp; }
    int getMp() const { return mp; }
    int getMaxMp() const { return maxMp; }

    void takeDamage(int dmg);
    bool consumeMp(int cost);
    void recoverHpMp(int hpAmount, int mpAmount);

    // ========== 经验等级系统 ==========
    void addExp(int amount);
    int getExp() const { return exp; }
    int getMaxExp() const { return maxExp; }
    int getLevel() const { return level; }

    // ========== 形态切换 ==========
    void setEnhanced(bool enhanced);
    bool getEnhanced() const { return isEnhanced; }
    void playCastAnimation(const QString &gifPath); // 播放施法动画，播放完自动恢复

signals:
    void levelUp(int newLevel);

private slots:
    void onFrameChanged(int frame);

private:
    TileMap *tileMap;
    qreal speed;
    QMovie *movie;

    int hp = 100;
    int maxHp = 100;
    int mp = 100;
    int maxMp = 100;

    int exp = 0;
    int maxExp = 100;
    int level = 1;

    // 动画状态
    bool facingRight = true;   // 当前朝向
    bool isRunning = false;    // 是否在跑动
    bool isEnhanced = false;   // 是否增强形态（10级）
    bool isCasting = false;    // 是否正在播放施法动画
    int castFrameCount = 0;    // 施法动画已播放帧数
    int castFrameTarget = 0;   // 施法动画总帧数
    QString currentGifPath;    // 当前播放的 GIF 路径

    void updateAnimationState(bool moving, bool right);
};

#endif // PLAYER_H
