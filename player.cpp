#include "player.h"
#include "tilemap.h"
#include <QPixmap>
#include <QDebug>
#include <QMovie>

Player::Player(TileMap *map, QGraphicsItem *parent)
    : QGraphicsPixmapItem(parent), tileMap(map), speed(4.0), movie(nullptr)
{
    // 尝试加载 GIF 动画
    movie = new QMovie(":/images/player.gif");
    if (movie->isValid()) {
        // GIF 原始尺寸 246x240，缩放到与原来静态图一致的 32x32
        movie->setScaledSize(QSize(32, 32));
        connect(movie, &QMovie::frameChanged, this, &Player::onFrameChanged);
        movie->start();
    } else {
        qDebug() << "Failed to load player.gif, falling back to static image.";
        delete movie;
        movie = nullptr;

        QPixmap pixmap(":/images/player.png");
        if (pixmap.isNull()) {
            qDebug() << "Failed to load player.png, creating blue placeholder.";
            pixmap = QPixmap(32, 32);
            pixmap.fill(Qt::blue);
        }
        setPixmap(pixmap);
    }
    setShapeMode(QGraphicsPixmapItem::BoundingRectShape);
}

Player::~Player()
{
    if (movie) {
        movie->stop();
        delete movie;
        movie = nullptr;
    }
}

void Player::onFrameChanged(int frame)
{
    Q_UNUSED(frame);
    if (movie) {
        setPixmap(movie->currentPixmap());
    }
}

void Player::takeDamage(int dmg)
{
    hp -= dmg;
    if (hp < 0) hp = 0;
    qDebug() << "Player took damage:" << dmg << "HP:" << hp << "/" << maxHp;
}

bool Player::consumeMp(int cost)
{
    if (mp < cost) {
        qDebug() << "Not enough MP! Need" << cost << "have" << mp;
        return false;
    }
    mp -= cost;
    qDebug() << "Consumed MP:" << cost << "MP left:" << mp << "/" << maxMp;
    return true;
}

void Player::recoverHpMp(int hpAmount, int mpAmount)
{
    hp += hpAmount;
    if (hp > maxHp) hp = maxHp;
    mp += mpAmount;
    if (mp > maxMp) mp = maxMp;
}

void Player::move(bool up, bool down, bool left, bool right)
{
    qreal dx = 0, dy = 0;
    if (left)  dx = -speed;
    if (right) dx =  speed;
    if (up)    dy = -speed;
    if (down)  dy =  speed;

    if (dx == 0 && dy == 0) return;

    setPos(x() + dx, y());
    if (tileMap->collidesWithWall(this))
        setPos(x() - dx, y());

    setPos(x(), y() + dy);
    if (tileMap->collidesWithWall(this))
        setPos(x(), y() - dy);
}