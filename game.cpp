#include "game.h"
#include "player.h"
#include "tilemap.h"
#include <QDebug>
#include <QGraphicsRectItem>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>
#include <QtMath>  // qSqrt

Game::Game(QWidget *parent)
    : QGraphicsView(parent),
      upPressed(false), downPressed(false), leftPressed(false), rightPressed(false),
      canTeleport(true), isTeleporting(false)
{
    scene = new QGraphicsScene(this);
    setScene(scene);
    setFixedSize(800, 600);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // 加载初始地图（使用 start 点）
    loadMap(":/maps/school_map.tmj", true);
}

Game::~Game()
{
    // 清理所有活跃的流星粒子（避免内存泄漏）
    for (Projectile *p : projectiles) {
        delete p;
    }
    projectiles.clear();

    // 清理所有刀浪
    for (BladeWave *bw : bladeWaves) {
        delete bw;
    }
    bladeWaves.clear();

    // 清理盾牌
    if (shieldItem) {
        delete shieldItem;
        shieldItem = nullptr;
    }
    shieldActive = false;

    // 清理所有敌人
    for (Enemy *e : enemies) {
        delete e;
    }
    enemies.clear();

    // 清理所有敌人炮弹
    for (EnemyProjectile *ep : enemyProjectiles) {
        delete ep;
    }
    enemyProjectiles.clear();

    // 清理 HUD
    if (hudHpBg) { delete hudHpBg; hudHpBg = nullptr; }
    if (hudHpFg) { delete hudHpFg; hudHpFg = nullptr; }
    if (hudMpBg) { delete hudMpBg; hudMpBg = nullptr; }
    if (hudMpFg) { delete hudMpFg; hudMpFg = nullptr; }
    if (hudText) { delete hudText; hudText = nullptr; }

    delete tileMap;
    delete player;
}

void Game::loadMap(const QString &mapFilePath, bool useStartPoint)
{
    // 清理旧地图
    if (tileMap) {
        delete tileMap;
        tileMap = nullptr;
    }
    if (player) {
        if (player->scene()) scene->removeItem(player);
        delete player;
        player = nullptr;
    }

    // 清理所有流星粒子
    for (Projectile *p : projectiles) {
        delete p;
    }
    projectiles.clear();

    // 清理所有刀浪
    for (BladeWave *bw : bladeWaves) {
        delete bw;
    }
    bladeWaves.clear();

    // 清理盾牌
    if (shieldItem) {
        delete shieldItem;
        shieldItem = nullptr;
    }
    shieldActive = false;

    // 清理所有敌人
    for (Enemy *e : enemies) {
        delete e;
    }
    enemies.clear();

    // 清理所有敌人炮弹
    for (EnemyProjectile *ep : enemyProjectiles) {
        delete ep;
    }
    enemyProjectiles.clear();

    // 清理 HUD
    if (hudHpBg) { delete hudHpBg; hudHpBg = nullptr; }
    if (hudHpFg) { delete hudHpFg; hudHpFg = nullptr; }
    if (hudMpBg) { delete hudMpBg; hudMpBg = nullptr; }
    if (hudMpFg) { delete hudMpFg; hudMpFg = nullptr; }
    if (hudText) { delete hudText; hudText = nullptr; }

    // 清空可攻击对象列表（旧的 Tile 会在下面的循环中被 delete）
    hittableItems.clear();

    // 清除场景中所有已有项（瓦片、碰撞体等）
    QList<QGraphicsItem*> items = scene->items();
    for (QGraphicsItem *item : items) {
        scene->removeItem(item);
        delete item;
    }

    // 创建新地图（负责 floor 和 wall 的渲染及碰撞）
    tileMap = new TileMap();
    if (!tileMap->loadFromFile(mapFilePath, scene)) {
        qDebug() << "Failed to load map:" << mapFilePath;
        // 失败回退：创建灰色背景和一个蓝色方块玩家
        QGraphicsRectItem *bg = new QGraphicsRectItem(0, 0, 800, 600);
        bg->setBrush(Qt::darkGray);
        scene->addItem(bg);
        player = new Player(nullptr);
        scene->addItem(player);
        player->setPos(100, 100);
        currentMapPath = mapFilePath;
        scene->setSceneRect(0, 0, 800, 600);
        setSceneRect(scene->sceneRect());
        centerOn(player);
        if (!gameTimer) {
            gameTimer = new QTimer(this);
            connect(gameTimer, &QTimer::timeout, this, &Game::updateGame);
            gameTimer->start(16);
        }
        return;
    }

    // ================= 手动绘制 door, chest, boss, portal 图层 =================
    // 读取地图 JSON 文件，解析这些特定图层
    QFile file(mapFilePath);
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray jsonData = file.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(jsonData);
        if (!doc.isNull()) {
            QJsonObject root = doc.object();
            int mapWidth = root["width"].toInt();
            int mapHeight = root["height"].toInt();
            int tileWidth = root["tilewidth"].toInt();
            int tileHeight = root["tileheight"].toInt();
            // 确保图块大小与 tileMap 一致（通常是32）
            Q_UNUSED(tileHeight);

            // 需要绘制的图层名称列表
            QStringList targetLayers = { "door", "chest", "boss_image", "portal_image", "minion_image" };
            // 为每个图层指定对应的图片资源
            QMap<QString, QString> layerImageMap;
            layerImageMap["door"] = ":/images/door.png";
            layerImageMap["chest"] = ":/images/chest.png";
            layerImageMap["boss_image"] = ":/images/boss.png";
            layerImageMap["portal_image"] = ":/images/portal.png";

            QJsonArray layers = root["layers"].toArray();
            for (const QJsonValue &layerVal : layers) {
                QJsonObject layerObj = layerVal.toObject();
                QString layerName = layerObj["name"].toString();
                if (!targetLayers.contains(layerName)) continue;

                QString imagePath = layerImageMap.value(layerName, "");
                if (imagePath.isEmpty()) {
                    qDebug() << "No image path for layer:" << layerName;
                    continue;
                }

                QJsonArray dataArr = layerObj["data"].toArray();
                if (dataArr.size() != mapWidth * mapHeight) {
                    qDebug() << "Layer data size mismatch for:" << layerName;
                    continue;
                }

                // 遍历图块数据
                for (int y = 0; y < mapHeight; ++y) {
                    for (int x = 0; x < mapWidth; ++x) {
                        int rawGid = dataArr[y * mapWidth + x].toInt();
                        // 清除高位标志（翻转/旋转）
                        int cleanGid = rawGid & 0x1FFFFFFF;
                        if (cleanGid == 0) continue; // 空图块

                        // minion_image 图层：创建会动的 Enemy，而不是静态 Tile
                        if (layerName == "minion_image") {
                            Enemy *enemy = new Enemy(tileMap, scene, QPointF(x * tileWidth, y * tileWidth), this);
                            enemies.append(enemy);
                            hittableItems.append(enemy);
                            continue; // 跳过 Tile 创建
                        }

                        // 创建 Tile 对象（Tile 类使用图片路径和坐标）
                        Tile *tile = new Tile(imagePath, x * tileWidth, y * tileWidth);
                        scene->addItem(tile);

                        // 标记可攻击对象：Boss 图层中的对象可被技能击中
                        if (layerName == "boss_image") {
                            tile->setData(0, "boss");      // 设置类型标识
                            hittableItems.append(tile);     // 加入可攻击列表
                        }
                    }
                }
                // qDebug() << "Manually drew layer:" << layerName;
            }
        } else {
            qDebug() << "Failed to parse JSON for manual layer drawing:" << mapFilePath;
        }
        file.close();
    } else {
        qDebug() << "Cannot open map file for manual drawing:" << mapFilePath;
    }

    // 创建玩家
    player = new Player(tileMap);
    scene->addItem(player);

    // 创建敌人（在地图中放置几个测试敌人）
    enemies.append(new Enemy(tileMap, scene, QPointF(400, 300), this));
    enemies.append(new Enemy(tileMap, scene, QPointF(700, 400), this));
    enemies.append(new Enemy(tileMap, scene, QPointF(500, 600), this));
    // 将敌人加入可攻击列表（技能可以击中敌人）
    for (Enemy *e : enemies) {
        hittableItems.append(e);
    }

    // 根据 useStartPoint 决定初始位置
    if (useStartPoint) {
        QPointF startPos = tileMap->getPlayerStart();
        if (startPos.isNull()) {
            startPos = QPointF(100, 100);
        }
        player->setPos(startPos);
    } else {
        // 临时置零，稍后由跨地图传送逻辑覆盖位置
        player->setPos(0, 0);
    }

    currentMapPath = mapFilePath;

    // 设置场景矩形（硬编码，可改为从地图获取）
    scene->setSceneRect(0, 0, 12800, 6400);
    setSceneRect(scene->sceneRect());

    // 只有在位置有效时才进行摄像头跟随（使用 start 点时已经设置位置，跨地图传送时暂不跟随）
    if (useStartPoint) {
        centerOn(player);
    }

    // 重置缩放为默认值
    zoomLevel = 1.0;
    applyZoom();

    // 创建 HUD（血蓝条）
    createHud();

    // 启动游戏循环（如果尚未启动）
    if (!gameTimer) {
        gameTimer = new QTimer(this);
        connect(gameTimer, &QTimer::timeout, this, &Game::updateGame);
        gameTimer->start(16);
    }
}

void Game::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_W: upPressed = true; break;
    case Qt::Key_S: downPressed = true; break;
    case Qt::Key_A: leftPressed = true; break;
    case Qt::Key_D: rightPressed = true; break;
    case Qt::Key_I: skillMeteorBurst(); break;   // ← I键：粒子爆发技能（一技能）
    case Qt::Key_J: skillNormalAttack(); break;  // ← J键：普攻（九宫格火光）
    case Qt::Key_K: skillFlashBlade(); break;    // ← K键：闪现刀浪技能（二技能）
    case Qt::Key_L: skillShieldActivate(); break;// ← L键：激活盾牌（三技能）
    case Qt::Key_Plus:
    case Qt::Key_Equal:  // 兼容主键盘 =/+ 键
        zoomLevel *= ZOOM_STEP;
        if (zoomLevel > MAX_ZOOM) zoomLevel = MAX_ZOOM;
        applyZoom();
        break;
    case Qt::Key_Minus:
        zoomLevel /= ZOOM_STEP;
        if (zoomLevel < MIN_ZOOM) zoomLevel = MIN_ZOOM;
        applyZoom();
        break;
    default: QGraphicsView::keyPressEvent(event);
    }
}

void Game::keyReleaseEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_W: upPressed = false; break;
    case Qt::Key_S: downPressed = false; break;
    case Qt::Key_A: leftPressed = false; break;
    case Qt::Key_D: rightPressed = false; break;
    case Qt::Key_L: skillShieldDeactivate(); break; // ← L键释放：关闭盾牌
    default: QGraphicsView::keyReleaseEvent(event);
    }
}

void Game::updateGame()
{
    // 仅在玩家存在时移动（防止空指针）
    if (player) {
        player->move(upPressed, downPressed, leftPressed, rightPressed);
        centerOn(player);
    }

    // 每 60 帧（约 1 秒）恢复 1 HP 和 1 MP
    regenCounter++;
    if (regenCounter >= 60) {
        regenCounter = 0;
        if (player) {
            player->recoverHpMp(1, 1);
        }
    }

    updateProjectiles();       // ← 更新所有流星粒子
    updateBladeWaves();        // ← 更新所有刀浪
    updateShieldPosition();    // ← 更新盾牌跟随玩家
    updateEnemies();           // ← 更新所有敌人
    updateEnemyProjectiles();  // ← 更新所有敌人炮弹
    updateHud();               // ← 更新 HUD 位置和数值
    checkPortal();
}

void Game::checkPortal()
{
    if (!canTeleport || isTeleporting) return;
    if (!player || !tileMap) return; // 安全检查

    QRectF playerRect = player->sceneBoundingRect();
    for (const Portal &portal : tileMap->getPortals()) {
        if (playerRect.intersects(portal.rect)) {
            canTeleport = false;
            isTeleporting = true;
            // 延迟执行传送，避免在遍历中删除对象
            QTimer::singleShot(0, this, [this, portal]() {
                performTeleport(portal);
            });
            break;
        }
    }
}

void Game::skillMeteorBurst()
{
    if (!player) return;
    if (!player->consumeMp(10)) return; // 消耗 10 MP，不足则无法释放

    // 以玩家中心为发射原点
    QPointF center = player->sceneBoundingRect().center();

    qreal speed = 10.0;      // 流星飞行速度（像素/帧）
    int damage = 25;         // 伤害值（预留，供后续血量系统使用）
    bool decay = true;       // 是否衰减（true=逐渐变小消失；改为 false 可测试不衰减模式）

    // 8 个方向：上、右上、右、右下、下、左下、左、左上
    QVector<QPointF> directions = {
        QPointF(0, -speed),                           // 上
        QPointF(speed * 0.707, -speed * 0.707),       // 右上
        QPointF(speed, 0),                            // 右
        QPointF(speed * 0.707, speed * 0.707),        // 右下
        QPointF(0, speed),                            // 下
        QPointF(-speed * 0.707, speed * 0.707),       // 左下
        QPointF(-speed, 0),                           // 左
        QPointF(-speed * 0.707, -speed * 0.707)       // 左上
    };

    for (const QPointF &dir : directions) {
        Projectile *p = new Projectile(center, dir, damage, decay, tileMap, scene);
        projectiles.append(p);
    }
}

void Game::updateProjectiles()
{
    // 倒序遍历，方便安全删除已死亡的粒子
    for (int i = projectiles.size() - 1; i >= 0; --i) {
        Projectile *p = projectiles[i];
        bool alive = p->update();

        // 检测是否击中可攻击对象（Boss、小怪等）
        if (alive) {
            for (QGraphicsItem *hittable : hittableItems) {
                if (p->collidesWithItem(hittable)) {
                    // 对 Enemy 造成实际伤害
                    Enemy *enemy = dynamic_cast<Enemy*>(hittable);
                    if (enemy) {
                        enemy->takeDamage(p->getDamage());
                    }
                    qDebug() << "Hit! Damage:" << p->getDamage()
                             << "to hittable object at" << hittable->pos();
                    alive = false;
                    break;
                }
            }
        }

        if (!alive) {
            delete p;
            projectiles.removeAt(i);
        }
    }
}

QPointF Game::getCurrentDirectionVector()
{
    qreal dx = 0.0;
    qreal dy = 0.0;
    if (rightPressed) dx += 1.0;
    if (leftPressed)  dx -= 1.0;
    if (downPressed)  dy += 1.0;
    if (upPressed)    dy -= 1.0;

    // 如果没有方向键被按下，默认向右
    if (dx == 0.0 && dy == 0.0) {
        dx = 1.0;
    }

    // 归一化（保证斜向速度大小与正方向一致）
    qreal len = qSqrt(dx * dx + dy * dy);
    if (len > 0.0) {
        dx /= len;
        dy /= len;
    }
    return QPointF(dx, dy);
}

void Game::skillFlashBlade()
{
    if (!player || !tileMap) return;
    if (!player->consumeMp(15)) return; // 消耗 15 MP，不足则无法释放

    // ========== 1. 检查是否有方向键被按下（静止时不触发）==========
    if (!upPressed && !downPressed && !leftPressed && !rightPressed) {
        return;
    }

    QPointF dir = getCurrentDirectionVector();

    // ========== 2. 闪现（步进法，不能穿墙）==========
    qreal flashDistance = 100.0; // 最大闪现距离
    qreal step = 4.0;            // 每步检测 4 像素
    QPointF oldCenter = player->sceneBoundingRect().center(); // 闪现前中心（轨迹用）
    QPointF oldPos = player->pos(); // 闪现前左上角（setPos 用）
    QPointF currentPos = oldPos;
    QPointF finalPos = oldPos;

    for (qreal dist = step; dist <= flashDistance; dist += step) {
        QPointF testPos = oldPos + QPointF(dir.x() * dist, dir.y() * dist);
        player->setPos(testPos);
        if (tileMap->collidesWithWall(player)) {
            // 撞墙了，停在当前安全位置
            player->setPos(currentPos);
            finalPos = currentPos;
            break;
        }
        currentPos = testPos;
        finalPos = testPos;
    }
    centerOn(player); // 闪现后摄像头跟随
    QPointF finalCenter = player->sceneBoundingRect().center(); // 闪现后中心（轨迹用）

    // ========== 3. 闪现轨迹红光效果 ==========
    int trailCount = 12;
    for (int i = 0; i < trailCount; ++i) {
        qreal ratio = static_cast<qreal>(i) / trailCount;
        QPointF trailPos = oldCenter + (finalCenter - oldCenter) * ratio;
        QGraphicsEllipseItem *dot = new QGraphicsEllipseItem(-4, -4, 8, 8);
        dot->setPos(trailPos);
        dot->setBrush(QBrush(QColor(255, 50, 50, 200)));
        dot->setPen(Qt::NoPen);
        scene->addItem(dot);
        // 200ms 后自动删除
        QTimer::singleShot(200, [dot]() { delete dot; });
    }

    // ========== 4. 射出刀浪 ==========
    qreal bladeSpeed = 12.0;   // 刀浪飞行速度（像素/帧）
    int damage = 40;           // 刀浪伤害值（比流星高）

    // 刀浪起始位置：玩家闪现后的前方一点
    QPointF bladeStart = player->sceneBoundingRect().center()
                         + QPointF(dir.x() * 20.0, dir.y() * 20.0);

    QPointF bladeVelocity(dir.x() * bladeSpeed, dir.y() * bladeSpeed);

    BladeWave *bw = new BladeWave(bladeStart, bladeVelocity, damage, tileMap, scene);
    bladeWaves.append(bw);
}

void Game::updateBladeWaves()
{
    // 倒序遍历，方便安全删除已死亡的刀浪
    for (int i = bladeWaves.size() - 1; i >= 0; --i) {
        BladeWave *bw = bladeWaves[i];
        bool alive = bw->update();

        // 检测是否击中可攻击对象（Boss、小怪等）
        if (alive) {
            for (QGraphicsItem *hittable : hittableItems) {
                if (bw->collidesWithItem(hittable)) {
                    // 对 Enemy 造成实际伤害
                    Enemy *enemy = dynamic_cast<Enemy*>(hittable);
                    if (enemy) {
                        enemy->takeDamage(bw->getDamage());
                    }
                    qDebug() << "BladeWave Hit! Damage:" << bw->getDamage()
                             << "to hittable object at" << hittable->pos();
                    alive = false;
                    break;
                }
            }
        }

        if (!alive) {
            delete bw;
            bladeWaves.removeAt(i);
        }
    }
}

void Game::skillNormalAttack()
{
    if (!player || !scene) return;

    // ========== 1. 九宫格攻击范围 ==========
    // 玩家中心 + 周围 3x3 瓦片区域 = 96x96 像素（中心 ±48）
    QPointF playerCenter = player->sceneBoundingRect().center();
    QRectF attackRect(playerCenter.x() - 48.0, playerCenter.y() - 48.0, 96.0, 96.0);

    // ========== 2. 火光闪烁视觉效果 ==========
    // 第一层：深色底框
    QGraphicsRectItem *fireBase = new QGraphicsRectItem(attackRect);
    fireBase->setBrush(QBrush(QColor(180, 60, 0, 120)));
    fireBase->setPen(QPen(QColor(255, 120, 0, 180), 2));
    scene->addItem(fireBase);

    // 第二层：中心亮光（稍小一点）
    QRectF innerRect(playerCenter.x() - 32.0, playerCenter.y() - 32.0, 64.0, 64.0);
    QGraphicsRectItem *fireInner = new QGraphicsRectItem(innerRect);
    fireInner->setBrush(QBrush(QColor(255, 180, 50, 160)));
    fireInner->setPen(Qt::NoPen);
    scene->addItem(fireInner);

    // 闪烁动画：50ms 变亮 → 100ms 变暗 → 150ms 删除
    QTimer::singleShot(50, [fireBase, fireInner]() {
        fireBase->setBrush(QBrush(QColor(255, 100, 0, 160)));
        fireBase->setPen(QPen(QColor(255, 220, 100, 220), 3));
        fireInner->setBrush(QBrush(QColor(255, 220, 100, 200)));
    });
    QTimer::singleShot(100, [fireBase, fireInner]() {
        fireBase->setBrush(QBrush(QColor(150, 50, 0, 80)));
        fireBase->setPen(QPen(QColor(255, 150, 0, 120), 2));
        fireInner->setBrush(QBrush(QColor(255, 150, 0, 100)));
    });
    QTimer::singleShot(200, [fireBase, fireInner]() {
        delete fireBase;
        delete fireInner;
    });

    // ========== 3. 伤害检测（九宫格范围内的 hittable 对象）==========
    for (QGraphicsItem *hittable : hittableItems) {
        QRectF hittableRect = hittable->boundingRect().translated(hittable->pos());
        if (attackRect.intersects(hittableRect)) {
            // 对 Enemy 造成实际伤害
            Enemy *enemy = dynamic_cast<Enemy*>(hittable);
            if (enemy) {
                enemy->takeDamage(15);
            }
            qDebug() << "Normal Attack Hit! Damage: 15"
                     << "to hittable object at" << hittable->pos();
        }
    }
}

void Game::skillShieldActivate()
{
    if (!player || !scene || shieldItem) return;
    if (!player->consumeMp(5)) return; // 开启盾牌消耗 5 MP

    // 创建盾牌：比玩家稍大的圆形（半径 28px）
    shieldItem = new QGraphicsEllipseItem(-28, -28, 56, 56);
    shieldItem->setPos(player->sceneBoundingRect().center());
    // 盾牌颜色：半透明青蓝色 + 发光边框
    shieldItem->setBrush(QBrush(QColor(100, 180, 255, 80)));
    shieldItem->setPen(QPen(QColor(150, 220, 255, 150), 3));
    scene->addItem(shieldItem);
    shieldActive = true;

    qDebug() << "Shield activated!";
}

void Game::skillShieldDeactivate()
{
    if (shieldItem) {
        delete shieldItem;
        shieldItem = nullptr;
    }
    shieldActive = false;

    qDebug() << "Shield deactivated.";
}

void Game::updateShieldPosition()
{
    if (shieldActive && shieldItem && player) {
        shieldItem->setPos(player->sceneBoundingRect().center());
    }
}

void Game::applyZoom()
{
    resetTransform();
    scale(zoomLevel, zoomLevel);
    if (player) {
        centerOn(player);
    }
}

void Game::createHud()
{
    if (!scene) return;

    // 血条背景（灰色）
    hudHpBg = new QGraphicsRectItem(0, 0, 120, 14);
    hudHpBg->setBrush(QBrush(QColor(60, 60, 60, 200)));
    hudHpBg->setPen(QPen(Qt::black, 1));
    scene->addItem(hudHpBg);

    // 血条前景（红色）
    hudHpFg = new QGraphicsRectItem(0, 0, 120, 14);
    hudHpFg->setBrush(QBrush(QColor(220, 60, 60, 230)));
    hudHpFg->setPen(Qt::NoPen);
    scene->addItem(hudHpFg);

    // 蓝条背景（灰色）
    hudMpBg = new QGraphicsRectItem(0, 0, 120, 14);
    hudMpBg->setBrush(QBrush(QColor(60, 60, 60, 200)));
    hudMpBg->setPen(QPen(Qt::black, 1));
    scene->addItem(hudMpBg);

    // 蓝条前景（蓝色）
    hudMpFg = new QGraphicsRectItem(0, 0, 120, 14);
    hudMpFg->setBrush(QBrush(QColor(60, 120, 220, 230)));
    hudMpFg->setPen(Qt::NoPen);
    scene->addItem(hudMpFg);

    // 文字
    hudText = new QGraphicsSimpleTextItem();
    hudText->setBrush(QBrush(Qt::white));
    QFont font = hudText->font();
    font.setPointSize(10);
    font.setBold(true);
    hudText->setFont(font);
    scene->addItem(hudText);
}

void Game::updateHud()
{
    if (!player || !hudHpBg) return;

    // 将视图左上角坐标转换为场景坐标，使 HUD 固定在屏幕左上角
    QPointF hudPos = mapToScene(10, 10);

    // 更新血条宽度
    qreal hpRatio = static_cast<qreal>(player->getHp()) / player->getMaxHp();
    if (hpRatio < 0) hpRatio = 0;
    hudHpFg->setRect(hudPos.x(), hudPos.y(), 120 * hpRatio, 14);
    hudHpBg->setRect(hudPos.x(), hudPos.y(), 120, 14);

    // 更新蓝条宽度
    qreal mpRatio = static_cast<qreal>(player->getMp()) / player->getMaxMp();
    if (mpRatio < 0) mpRatio = 0;
    hudMpFg->setRect(hudPos.x(), hudPos.y() + 18, 120 * mpRatio, 14);
    hudMpBg->setRect(hudPos.x(), hudPos.y() + 18, 120, 14);

    // 更新文字
    QString text = QString("HP:%1/%2  MP:%3/%4")
                       .arg(player->getHp()).arg(player->getMaxHp())
                       .arg(player->getMp()).arg(player->getMaxMp());
    hudText->setText(text);
    hudText->setPos(hudPos.x() + 2, hudPos.y() + 34);

    // 确保 HUD 在最上层
    hudHpBg->setZValue(1000);
    hudHpFg->setZValue(1001);
    hudMpBg->setZValue(1000);
    hudMpFg->setZValue(1001);
    hudText->setZValue(1002);
}

void Game::addEnemyProjectile(EnemyProjectile *ep)
{
    if (ep) {
        enemyProjectiles.append(ep);
    }
}

void Game::updateEnemies()
{
    // 倒序遍历，方便安全删除已死亡的敌人
    for (int i = enemies.size() - 1; i >= 0; --i) {
        Enemy *e = enemies[i];
        e->update();
        if (e->isDead()) {
            // 从可攻击列表中移除
            hittableItems.removeAll(e);
            delete e;
            enemies.removeAt(i);
        }
    }
}

void Game::updateEnemyProjectiles()
{
    // 倒序遍历，方便安全删除已死亡的炮弹
    for (int i = enemyProjectiles.size() - 1; i >= 0; --i) {
        EnemyProjectile *ep = enemyProjectiles[i];
        bool alive = ep->update(tileMap);

        // 检测是否击中玩家
        if (alive && player && ep->collidesWithItem(player)) {
            // 如果盾牌激活，阻挡伤害
            if (shieldActive) {
                qDebug() << "Enemy projectile blocked by shield!";
            } else {
                player->takeDamage(ep->getDamage());
                qDebug() << "Player hit by enemy! Damage:" << ep->getDamage();
            }
            alive = false;
        }

        if (!alive) {
            delete ep;
            enemyProjectiles.removeAt(i);
        }
    }
}

void Game::performTeleport(const Portal &portal)
{
    // 二次确认玩家仍与传送门重叠（防止延迟期间玩家离开）
    if (!player || !tileMap) {
        // 意外情况，恢复标志
        isTeleporting = false;
        QTimer::singleShot(500, this, [this]() { canTeleport = true; });
        return;
    }

    QRectF playerRect = player->sceneBoundingRect();
    bool stillIntersects = false;
    for (const Portal &p : tileMap->getPortals()) {
        if (playerRect.intersects(p.rect)) {
            stillIntersects = true;
            break;
        }
    }
    if (!stillIntersects) {
        // 玩家已离开，取消传送
        isTeleporting = false;
        QTimer::singleShot(500, this, [this]() { canTeleport = true; });
        return;
    }

    // 判断同地图还是跨地图
    if (portal.targetMap.isEmpty() || portal.targetMap == currentMapPath) {
        // 同地图传送：直接移动玩家
        for (const Portal &p : tileMap->getPortals()) {
            if (p.id == portal.targetPortalId) {
                player->setPos(p.rect.center());
                centerOn(player);
                break;
            }
        }
        // 恢复冷却
        QTimer::singleShot(2000, this, [this]() {
            canTeleport = true;
            isTeleporting = false;
        });
    } else {
        // 跨地图传送
        QString newMapPath = portal.targetMap;
        // 加载新地图，但不要自动设置 start 点
        loadMap(newMapPath, false);
        // 在新地图中查找目标传送门，设置玩家位置
        bool found = false;
        for (const Portal &p : tileMap->getPortals()) {
            if (p.id == portal.targetPortalId) {
                player->setPos(p.rect.center());
                centerOn(player);
                found = true;
                break;
            }
        }
        if (!found) {
            // 如果找不到目标传送门，使用 start 点作为后备
            QPointF startPos = tileMap->getPlayerStart();
            if (!startPos.isNull()) {
                player->setPos(startPos);
                centerOn(player);
            } else {
                qDebug() << "Warning: target portal not found, and no start point.";
            }
        }
        // 跨地图冷却稍长
        QTimer::singleShot(5000, this, [this]() {
            canTeleport = true;
            isTeleporting = false;
        });
    }
}