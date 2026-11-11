// The sky, star, boom and cloud bears (ennemi_bisou_ciel.cpp,
// ennemi_bisou_etoile.cpp, ennemi_bisou_boom.cpp, ennemi_bisou_nuage.cpp) and
// their gore pieces (gore_pieds_bisou_ciel.h, gore_pieds_bisou_etoile.h,
// gore_bisou_boom.h).

#include "carebears/carebears_world.hpp"

namespace bb::game::carebears {

namespace {

const int kBloodX[] = {0, 7, 10, 7, 0, -7, -10, -7};
const int kBloodY[] = {-15, -25, -25, -25, -35, -25, -25, -25};

const HalvedBearImages kSkyImages = {img::kSkyWalkRight,   img::kSkyWalkLeft,     img::kSkyDieRight,
                                     img::kSkyDieLeft,     img::kSkyTopFallRight, img::kSkyTopFallLeft,
                                     img::kSkyTopEndRight, img::kSkyTopEndLeft};
const HalvedBearImages kStarImages = {img::kStarWalkRight,   img::kStarWalkLeft,     img::kStarDieRight,
                                      img::kStarDieLeft,     img::kStarTopFallRight, img::kStarTopFallLeft,
                                      img::kStarTopEndRight, img::kStarTopEndLeft};

}  // namespace

//-----------------------------------------------------------------------------
// HalvedBear
//-----------------------------------------------------------------------------

void HalvedBear::update() {
    if (bleeding > 0) {
        bleeding -= 1;
    }
    beforeUpdate();

    switch (mode) {
        case kNormal:
        case kWalking:
            walkAbout();
            break;
        case kDying:
            dieWhole();
            break;
        case kDying2:
            topFalls();
            break;
        case kDying3:
            topLands();
            break;
        case kShooting:
            afterShot();
            break;
        case kCarbonised:
            burnUp();
            break;
        default:
            break;
    }
    dieWhenOffscreen();
}

void HalvedBear::walkImage(int step) {
    image = enemyImage((direction == kFacingRight ? images_.walkRight : images_.walkLeft) + step);
}

void HalvedBear::dieWhole() {
    fall();

    animation.tick(5);

    if (animation.frame == 4) {
        // Cut in two: the legs stay, the top falls.
        dropLegs();
        y -= 19;
        dy = 0;
        animation.restart();
        mode = kDying2;
        topFalls();
    } else {
        image =
            enemyImage((direction == kFacingRight ? images_.dieRight : images_.dieLeft) + animation.frame);
    }
}

void HalvedBear::topFalls() {
    animation.tickUpTo(6, 3);

    if (direction == kFacingRight) {
        image = enemyImage(images_.topFallRight + animation.frame);
        x += 1;
    } else {
        image = enemyImage(images_.topFallLeft + animation.frame);
        x -= 1;
    }

    fall();
    if (platformAt(x, y) != 0 && animation.frame >= 3) {
        animation.restart();
        mode = kDying3;
        topLands();
    }
}

void HalvedBear::topLands() {
    animation.tickUpTo(6, 3);

    image =
        enemyImage((direction == kFacingRight ? images_.topEndRight : images_.topEndLeft) + animation.frame);

    if (animation.frame >= 3) {
        world().media.effects->engrave(x, y, image);
        dead = true;
    }
}

void HalvedBear::burnUp() {
    burn(img::kBearBurnLeft, img::kBearBurnRight, 7, 4);
}

void HalvedBear::hitBy(Shot& shot) {
    Enemy::hitBy(shot);
    sprayBlood(shot, kBloodX, kBloodY);

    if (mode == kDying) {
        world().media.levelSounds->play(snd::kBearDeath + world().random() % 9);  // one of 9
    }
}

//-----------------------------------------------------------------------------
// The sky bear
//-----------------------------------------------------------------------------

SkyBear::SkyBear(World& world) : HalvedBear(world, kSkyImages) {
    health.points = 175;
}

void SkyBear::beforeUpdate() {
    if (peeWait_ > 0) {
        peeWait_ -= 1;
    }
}

void SkyBear::walkAbout() {
    static const int kSpeed = 1;
    World& w = world();

    fall();

    animation.cycle(4, 8);

    if (x - kSpeed < minX || solidWall(x - kSpeed, y)) {
        direction = kFacingRight;
    } else if (x + kSpeed > w.camera.right() || solidWall(x + kSpeed, y)) {
        direction = kFacingLeft;
    }
    walk(direction == kFacingRight ? kSpeed : -kSpeed);
    walkImage(animation.frame);

    // Pees a rainbow on a player in front of it, on the ground only
    // (step 4 of the walk "looks nicer").
    if (peeWait_ == 0 && animation.frame == 4 && w.target != nullptr && platformAt(x, y) != 0) {
        const int dx = x - w.target->x;
        const int dy2 = y - w.target->y;
        if (dy2 > -10 && dy2 < 20) {
            const bool left = direction == kFacingLeft;
            if ((left && dx > 0 && dx < 90) || (!left && dx < 0 && dx > -90)) {
                w.media.levelSounds->play(snd::kRainbow);
                auto* rainbow = new Rainbow(w);
                rainbow->x = left ? x - 15 : x + 15;
                rainbow->y = y - 25;
                rainbow->direction = direction;
                w.enemyShots.add(rainbow);
                animation.subFrame = 0;
                mode = kShooting;
            }
        }
    }
    hitboxFromImage();
}

void SkyBear::afterShot() {
    animation.subFrame += 1;
    if (animation.subFrame >= 60) {
        animation.subFrame = 0;
        mode = kWalking;
        peeWait_ = 150;
    }
}

void SkyBear::dropLegs() {
    auto* legs = new WalkingLegs(world(), img::kSkyLegsRight, img::kSkyLegsLeft);
    legs->x = x;
    legs->direction = direction;
    legs->y = y;
    world().gore.add(legs);
}

//-----------------------------------------------------------------------------
// The star bear
//-----------------------------------------------------------------------------

StarBear::StarBear(World& world) : HalvedBear(world, kStarImages) {
    health.points = 200;
    shotDelay_ = 200 + world.random() % 250;
}

void StarBear::beforeUpdate() {
    shotWait_ += 1;
}

void StarBear::walkAbout() {
    static const int kSpeed = 1;
    World& w = world();
    const int camera = w.camera.x;
    const int cameraRight = w.camera.right();

    fall();

    animation.cycle(4, 8);

    // Stays on its platform (and looks 40 pixels beyond the right edge).
    if (x - kSpeed < minX || solidWall(x - kSpeed, y) ||
        (platformAt(x - kSpeed, y) == 0 && platformAt(x - kSpeed, y + 5) == 0)) {
        direction = kFacingRight;
    } else if (x + kSpeed > cameraRight + 40 || solidWall(x + kSpeed, y) ||
               (platformAt(x + kSpeed, y) == 0 && platformAt(x + kSpeed, y + 5) == 0)) {
        direction = kFacingLeft;
    }
    walk(direction == kFacingRight ? kSpeed : -kSpeed);
    walkImage(animation.frame);

    // 2002: the shot counter also advances here, so twice per tick.
    shotWait_ += 1;
    if (animation.frame == 4 && shotWait_ >= shotDelay_ && x > camera) {
        auto* star = new BouncingStar(w);
        star->x = x;
        star->y = y - 30;
        star->direction = direction;
        w.enemyShots.add(star);
        animation.subFrame = 0;
        mode = kShooting;
    }
    hitboxFromImage();
}

void StarBear::afterShot() {
    animation.subFrame += 1;
    if (animation.subFrame >= 60) {
        animation.subFrame = 0;
        mode = kWalking;
        shotDelay_ = 200 + world().random() % 500;
        shotWait_ = 0;
    }
}

void StarBear::dropLegs() {
    auto* legs = new DancingLegs(world());
    legs->x = x;
    legs->direction = direction;
    legs->y = y;
    world().gore.add(legs);
}

//-----------------------------------------------------------------------------
// The boom bear
//-----------------------------------------------------------------------------

void BoomBear::update() {
    if (bleeding > 0) {
        bleeding -= 1;
    }
    switch (mode) {
        case kNormal:
        case kWalking:
            walkAbout();
            break;
        case kDying:
        case kCarbonised:
            explode();
            break;
        default:
            break;
    }
    dieWhenOffscreen();
}

void BoomBear::walkAbout() {
    static const int kSpeed = 3;
    World& w = world();

    fall();

    animation.cycle(3, 8);

    if (x - kSpeed < minX || solidWall(x - kSpeed, y)) {
        direction = kFacingRight;
    } else if (x + kSpeed > w.camera.right() || solidWall(x + kSpeed, y)) {
        direction = kFacingLeft;
    }
    if (direction == kFacingRight) {
        walk(kSpeed);
        image = enemyImage(img::kBoomWalkRight + animation.frame);
    } else {
        walk(-kSpeed);
        image = enemyImage(img::kBoomWalkLeft + animation.frame);
    }

    // Close enough to the target: kamikaze!
    if (w.target != nullptr) {
        const int dx = x - w.target->x;
        const int dy2 = y - w.target->y;
        if (dx > -60 && dx < 60 && dy2 > -40 && dy2 < 20) {
            animation.restart();
            mode = kDying;
            w.media.miscSounds->play(miscsnd::kExplosion);
            explode();
        }
    }
    hitboxFromImage();
}

void BoomBear::explode() {
    World& w = world();
    const bool right = direction == kFacingRight;

    animation.tick(4);

    if (animation.frame == 7 && animation.subFrame == 0) {
        auto* foot = new BoomPiece(w, img::kBoomFootRight, img::kBoomFootLeft, -2);
        foot->x = right ? x - 60 : x + 60;
        foot->y = right ? y - 22 : y - 26;
        foot->direction = direction;
        foot->dy = -(w.random() % 4);
        w.gore.add(foot);
        auto* head = new BoomPiece(w, img::kBoomHeadRight, img::kBoomHeadLeft, 2);
        head->x = right ? x + 46 : x - 45;
        head->y = right ? y - 70 : y - 62;
        head->direction = direction;
        head->dy = -1;
        w.gore.add(head);
    } else if (animation.frame == 8 && animation.subFrame == 0) {
        auto* belly = new BoomPiece(w, img::kBoomBellyRight, img::kBoomBellyLeft, 1);
        belly->x = right ? x + 59 : x - 58;
        belly->y = right ? y - 3 : y - 7;
        belly->direction = direction;
        belly->dy = 0;
        w.gore.add(belly);
    } else if (animation.frame >= 10) {
        dead = true;
    } else {
        image = enemyImage((right ? img::kBoomExplodeRight : img::kBoomExplodeLeft) + animation.frame);
        hitboxFromImage();
    }
}

void BoomBear::hitBy(Shot& shot) {
    if (mode == kDying || mode == kCarbonised) {
        return;  // already blowing up
    }
    Enemy::hitBy(shot);
    sprayBlood(shot, kBloodX, kBloodY);
    if (mode == kDying || mode == kCarbonised) {
        world().media.miscSounds->play(miscsnd::kExplosion);
    }
}

//-----------------------------------------------------------------------------
// The cloud bear
//-----------------------------------------------------------------------------

CloudBear::CloudBear(World& world) : Enemy(world) {
    health.points = 250;
    shotDelay_ = 150 + world.random() % 150;
}

void CloudBear::update() {
    if (bleeding > 0) {
        bleeding -= 1;
    }
    shotWait_ += 1;

    switch (mode) {
        case kNormal:
        case kWalking:
            sit();
            break;
        case kDying:
        case kCarbonised:
            hitWait();
            break;
        case kDying2:
            fallDown();
            break;
        case kDying4:
            finish();
            break;
        default:
            break;
    }
    dieWhenOffscreen();
}

void CloudBear::sit() {
    static const int kBob[] = {0, 1, 2, 1};
    World& w = world();

    // Faces the middle of the screen.
    if (x > w.camera.centre()) {
        direction = kFacingLeft;
        image = enemyImage(img::kCloudLeft + animation.loop(kBob, 4, 10));
    } else {
        direction = kFacingRight;
        image = enemyImage(img::kCloudRight + animation.loop(kBob, 4, 10));
    }

    if (shotWait_ >= shotDelay_ && x <= w.camera.right()) {
        auto* cloud = new StormCloud(w);
        cloud->x = x;
        cloud->y = y - 17;
        cloud->direction = direction;
        w.enemyShots.add(cloud);
        shotWait_ = 0;
        shotDelay_ = 250 + w.random() % 250;
    }
    hitboxFromImage();
}

void CloudBear::hitWait() {
    animation.frame += 1;
    image = enemyImage(direction == kFacingLeft ? img::kCloudHitLeft : img::kCloudHitRight);
    if (animation.frame >= 30) {
        animation.restart();
        mode = kDying2;
    }
}

void CloudBear::fallDown() {
    fall();

    animation.cycle(8, 2);
    image =
        enemyImage((direction == kFacingLeft ? img::kCloudFallLeft : img::kCloudFallRight) + animation.frame);

    if (platformAt(x, y) != 0) {
        animation.restart();
        mode = kDying4;
    }
}

void CloudBear::finish() {
    if (animation.frame < 6) {
        animation.tick(3);
        image = enemyImage((direction == kFacingRight ? img::kCloudEndRight : img::kCloudEndLeft) +
                           animation.frame);
    } else {
        world().media.effects->engrave(x, y, image);
        dead = true;
    }
}

//-----------------------------------------------------------------------------
// Gore
//-----------------------------------------------------------------------------

void WalkingLegs::update() {
    fall();

    if (animation.frame < 3) {
        animation.tick(6);
    }

    if (direction == kFacingRight) {
        walk(-1);
        image = enemyImage(firstRight_ + animation.frame);
    } else {
        walk(1);
        image = enemyImage(firstLeft_ + animation.frame);
    }

    if (animation.frame == 3 && platformAt(x, y) != 0) {
        world().media.effects->engrave(x, y, image);
        dead = true;
    }
    dieWhenFallenOut();
}

void DancingLegs::update() {
    static const int kDance[] = {0, 1, 2, 1};

    if (!dancing_) {
        fall();

        if (animation.frame < 3) {
            animation.tick(6);
        }

        if (direction == kFacingRight) {
            walk(-1);
            image = enemyImage(img::kStarLegsRight + animation.frame);
        } else {
            walk(1);
            image = enemyImage(img::kStarLegsLeft + animation.frame);
        }

        if (animation.frame == 3 && platformAt(x, y) != 0) {
            dancing_ = true;
            animation.restart();
        }
        dieWhenFallenOut();
        return;
    }

    ticks_ += 1;
    image = enemyImage((direction == kFacingRight ? img::kStarLegsDanceRight : img::kStarLegsDanceLeft) +
                       animation.loop(kDance, 4, 5));
    if (ticks_ >= 100) {
        world().media.effects->engrave(x, y, image);
        dead = true;
    }
}

void BoomPiece::update() {
    World& w = world();
    fall();

    if (animation.frame < 5) {
        animation.tick(8);
        image = enemyImage((direction == kFacingRight ? firstRight_ : firstLeft_) + animation.frame);
    }

    if ((platformAt(x, y) != 0 && animation.frame >= 5) || solidWall(x, y)) {
        w.media.effects->engrave(x, y, image);
        dead = true;
        w.media.miscSounds->play(miscsnd::kGoreSplat + w.random() % 2);  // one of 2
    } else {
        x += direction == kFacingRight ? speedRight_ : -speedRight_;
    }
    dieWhenOffscreen();
}

}  // namespace bb::game::carebears
