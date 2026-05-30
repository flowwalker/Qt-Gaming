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

    // ========== 血量蓝量系统 ==========
    int getHp() const { return hp; }
    int getMaxHp() const { return maxHp; }
    int getMp() const { return mp; }
    int getMaxMp() const { return maxMp; }

    void takeDamage(int dmg);
    bool consumeMp(int cost);
    void recoverHpMp(int hpAmount, int mpAmount);

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
};

#endif // PLAYER_H