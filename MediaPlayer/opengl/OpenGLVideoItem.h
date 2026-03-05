#pragma once

#include <QQuickFramebufferObject>
#include <QtQml/qqml.h>
#include <QImage>
#include <QMutex>

class OpenGLVideoRenderer;

class OpenGLVideoItem : public QQuickFramebufferObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool flipX READ flipX WRITE setFlipX NOTIFY flipXChanged)
    Q_PROPERTY(bool flipY READ flipY WRITE setFlipY NOTIFY flipYChanged)
    Q_PROPERTY(bool preserveAspectRatio READ preserveAspectRatio WRITE setPreserveAspectRatio NOTIFY preserveAspectRatioChanged)

public:
    explicit OpenGLVideoItem(QQuickItem *parent = nullptr);

    Renderer *createRenderer() const override;

    Q_INVOKABLE void setFrame(const QImage &frame);

    bool flipX() const;
    void setFlipX(bool enabled);

    bool flipY() const;
    void setFlipY(bool enabled);

    bool preserveAspectRatio() const;
    void setPreserveAspectRatio(bool enabled);

    bool takePendingFrame(QImage &out);
    bool queryRenderOptions(bool &flipX, bool &flipY, bool &preserveAspectRatio);

signals:
    void flipXChanged();
    void flipYChanged();
    void preserveAspectRatioChanged();

private:
    mutable QMutex m_frameMutex;
    QImage m_pendingFrame;
    bool m_dirty = false;
    bool m_flipX = false;
    bool m_flipY = false;
    bool m_preserveAspectRatio = true;
};
