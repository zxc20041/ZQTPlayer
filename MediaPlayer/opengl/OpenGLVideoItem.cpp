#include "OpenGLVideoItem.h"

#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QOpenGLFramebufferObject>
#include <QDebug>

class OpenGLVideoRenderer : public QQuickFramebufferObject::Renderer, protected QOpenGLFunctions
{
public:
    OpenGLVideoRenderer()
    {
        initializeOpenGLFunctions();
        initProgram();
    }

    ~OpenGLVideoRenderer() override
    {
        delete m_texture;
        delete m_program;
    }

    QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) override
    {
        QOpenGLFramebufferObjectFormat format;
        format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
        format.setSamples(0);
        return new QOpenGLFramebufferObject(size, format);
    }

    void synchronize(QQuickFramebufferObject *item) override
    {
        auto *videoItem = static_cast<OpenGLVideoItem *>(item);
        QImage frame;
        bool flipX = false;
        bool flipY = false;
        bool preserveAspectRatio = true;
        if (videoItem->takePendingFrame(frame)) {
            m_pendingFrame = frame.convertToFormat(QImage::Format_RGBA8888);
            m_hasNewFrame = true;
        }
        videoItem->queryRenderOptions(flipX, flipY, preserveAspectRatio);
        m_flipX = flipX;
        m_flipY = flipY;
        m_preserveAspectRatio = preserveAspectRatio;
    }

    void render() override
    {
        const QSize viewport = framebufferObject()->size();
        glViewport(0, 0, viewport.width(), viewport.height());
        glDisable(GL_DEPTH_TEST);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        if (m_hasNewFrame) {
            updateTexture();
            m_hasNewFrame = false;
        }

        if (!m_texture || !m_program || !m_program->isLinked()) {
            update();
            return;
        }

        GLfloat sx = 1.0f;
        GLfloat sy = 1.0f;
        if (m_preserveAspectRatio && m_texWidth > 0 && m_texHeight > 0 && viewport.height() > 0) {
            const float videoAspect = static_cast<float>(m_texWidth) / static_cast<float>(m_texHeight);
            const float viewAspect = static_cast<float>(viewport.width()) / static_cast<float>(viewport.height());
            if (viewAspect > videoAspect) {
                sx = videoAspect / viewAspect;
            } else {
                sy = viewAspect / videoAspect;
            }
        }

        const GLfloat vertices[] = {
            -sx, -sy,
             sx, -sy,
            -sx,  sy,
             sx,  sy
        };

        static const GLfloat texCoords[] = {
            0.0f, 0.0f,
            1.0f, 0.0f,
            0.0f, 1.0f,
            1.0f, 1.0f
        };

        m_program->bind();
        m_texture->bind(0);
        m_program->setUniformValue("uTexture", 0);
        m_program->setUniformValue("uFlipX", m_flipX ? 1 : 0);
        m_program->setUniformValue("uFlipY", m_flipY ? 1 : 0);

        m_program->enableAttributeArray(0);
        m_program->enableAttributeArray(1);
        m_program->setAttributeArray(0, GL_FLOAT, vertices, 2);
        m_program->setAttributeArray(1, GL_FLOAT, texCoords, 2);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        m_program->disableAttributeArray(0);
        m_program->disableAttributeArray(1);
        m_texture->release();
        m_program->release();

        update();
    }

private:
    void initProgram()
    {
        m_program = new QOpenGLShaderProgram();

        static const char *vs = R"(
            attribute vec2 aPos;
            attribute vec2 aTex;
            varying vec2 vTex;
            void main() {
                gl_Position = vec4(aPos, 0.0, 1.0);
                vTex = aTex;
            }
        )";

        static const char *fs = R"(
            varying mediump vec2 vTex;
            uniform sampler2D uTexture;
            uniform int uFlipX;
            uniform int uFlipY;
            void main() {
                mediump vec2 uv = vTex;
                if (uFlipX == 1) uv.x = 1.0 - uv.x;
                if (uFlipY == 1) uv.y = 1.0 - uv.y;
                gl_FragColor = texture2D(uTexture, uv);
            }
        )";

        if (!m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, vs)) {
            qWarning() << "OpenGLVideoItem vertex shader compile failed:" << m_program->log();
        }
        if (!m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, fs)) {
            qWarning() << "OpenGLVideoItem fragment shader compile failed:" << m_program->log();
        }
        m_program->bindAttributeLocation("aPos", 0);
        m_program->bindAttributeLocation("aTex", 1);
        if (!m_program->link()) {
            qWarning() << "OpenGLVideoItem shader link failed:" << m_program->log();
        }
    }

    void updateTexture()
    {
        if (m_pendingFrame.isNull()) return;

        if (!m_texture || m_texWidth != m_pendingFrame.width() || m_texHeight != m_pendingFrame.height()) {
            delete m_texture;
            m_texture = new QOpenGLTexture(QOpenGLTexture::Target2D);
            m_texture->create();
            m_texture->setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
            m_texture->setWrapMode(QOpenGLTexture::ClampToEdge);
            m_texture->setFormat(QOpenGLTexture::RGBA8_UNorm);
            m_texture->setSize(m_pendingFrame.width(), m_pendingFrame.height());
            m_texture->allocateStorage(QOpenGLTexture::RGBA, QOpenGLTexture::UInt8);
            m_texWidth = m_pendingFrame.width();
            m_texHeight = m_pendingFrame.height();
        }

        m_texture->setData(QOpenGLTexture::RGBA, QOpenGLTexture::UInt8, m_pendingFrame.constBits());
    }

private:
    QOpenGLShaderProgram *m_program = nullptr;
    QOpenGLTexture *m_texture = nullptr;
    QImage m_pendingFrame;
    bool m_hasNewFrame = false;
    int m_texWidth = 0;
    int m_texHeight = 0;
    bool m_flipX = false;
    bool m_flipY = false;
    bool m_preserveAspectRatio = true;
};

OpenGLVideoItem::OpenGLVideoItem(QQuickItem *parent)
    : QQuickFramebufferObject(parent)
{
}

QQuickFramebufferObject::Renderer *OpenGLVideoItem::createRenderer() const
{
    return new OpenGLVideoRenderer();
}

void OpenGLVideoItem::setFrame(const QImage &frame)
{
    if (frame.isNull()) return;

    {
        QMutexLocker locker(&m_frameMutex);
        m_pendingFrame = frame;
        m_dirty = true;
    }

    update();
}

bool OpenGLVideoItem::flipX() const
{
    QMutexLocker locker(&m_frameMutex);
    return m_flipX;
}

void OpenGLVideoItem::setFlipX(bool enabled)
{
    {
        QMutexLocker locker(&m_frameMutex);
        if (m_flipX == enabled) return;
        m_flipX = enabled;
    }
    emit flipXChanged();
    update();
}

bool OpenGLVideoItem::flipY() const
{
    QMutexLocker locker(&m_frameMutex);
    return m_flipY;
}

void OpenGLVideoItem::setFlipY(bool enabled)
{
    {
        QMutexLocker locker(&m_frameMutex);
        if (m_flipY == enabled) return;
        m_flipY = enabled;
    }
    emit flipYChanged();
    update();
}

bool OpenGLVideoItem::preserveAspectRatio() const
{
    QMutexLocker locker(&m_frameMutex);
    return m_preserveAspectRatio;
}

void OpenGLVideoItem::setPreserveAspectRatio(bool enabled)
{
    {
        QMutexLocker locker(&m_frameMutex);
        if (m_preserveAspectRatio == enabled) return;
        m_preserveAspectRatio = enabled;
    }
    emit preserveAspectRatioChanged();
    update();
}

bool OpenGLVideoItem::takePendingFrame(QImage &out)
{
    QMutexLocker locker(&m_frameMutex);
    if (!m_dirty) return false;

    out = m_pendingFrame;
    m_dirty = false;
    return true;
}

bool OpenGLVideoItem::queryRenderOptions(bool &flipX, bool &flipY, bool &preserveAspectRatio)
{
    QMutexLocker locker(&m_frameMutex);
    flipX = m_flipX;
    flipY = m_flipY;
    preserveAspectRatio = m_preserveAspectRatio;
    return true;
}
