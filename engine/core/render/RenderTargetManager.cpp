#include "engine/core/log/Log.h"
#include "RenderTargetManager.h"
#include "Material.h"
#include "engine/core/main/Engine.h"
#include "render/ShaderProgramRes.h"
#include "TextureRes.h"
#include <algorithm>

namespace Echo
{
	// 构造函数
	RenderTargetManager::RenderTargetManager()
		:  m_currentRenderTarget( RTI_End )
	{
	}

	// 析构函数
	RenderTargetManager::~RenderTargetManager()
	{
		for( RenderTargetMap::iterator bit = m_renderTargets.begin(); bit != m_renderTargets.end();  )
		{
			EchoSafeDelete( bit->second, RenderTarget);
			m_renderTargets.erase(bit ++);
		}
	}

	// 获取渲染实例
	RenderTargetManager* RenderTargetManager::instance()
	{
		static RenderTargetManager* inst = EchoNew(RenderTargetManager);
		return inst;
	}

	// 初始化
	bool RenderTargetManager::initialize()
	{
		RenderTarget::Options option;
		option.depth = true;
		RenderTarget* defautRT = Renderer::instance()->createRenderTarget(RTI_DefaultBackBuffer, Renderer::instance()->getScreenWidth(), Renderer::instance()->getScreenHeight(), Renderer::instance()->getBackBufferPixelFormat(), option);
		if (defautRT)
		{
			defautRT->doStoreDefaultRenderTarget();
			m_renderTargets.insert(RenderTargetMap::value_type(RTI_DefaultBackBuffer, defautRT));

			return true;
		}

		EchoLogError("EchoNew( RTI_DefaultBackBuffer ) Failed !");
		return false;
	}

	void RenderTargetManager::destroyRenderTargetByID(ui32 _id)
	{
		RenderTargetMap::iterator fit = m_renderTargets.find( _id );

		if( fit == m_renderTargets.end() )
		{
			EchoLogError( "Could not found RenderTarget[%d]", _id );
			return;
		}

		if (Renderer::instance()->isEnableFrameProfile())
		{
			ui32 size = (fit->second)->getMemorySize();
			Renderer::instance()->getFrameState().decrRendertargetSize(size);
		}

		EchoSafeDelete(fit->second, RenderTarget);
		m_renderTargets.erase(fit);
	}

	// 开始渲染目标
	bool RenderTargetManager::beginRenderTarget( ui32 _id,bool _clearColor, const Color& _backgroundColor, bool _clearDepth, float _depthValue, bool _clearStencil, ui8 stencilValue, ui32 rbo)
	{
		EchoAssert( _id != RTI_End );

		RenderTarget* pRT = getRenderTargetByID( _id );

		RenderTarget* pUsingRT = 0;

		if(m_currentRenderTarget != RTI_End)
		{
			pUsingRT = getRenderTargetByID( m_currentRenderTarget );
		}

		EchoAssert( pRT );

		if( m_currentRenderTarget != _id )
		{
			pRT->m_bFrameBufferChange = true;
		}
		else
		{
			pRT->m_bFrameBufferChange = false;
		}

		if( m_currentRenderTarget == RTI_End )
		{
			pRT->m_bViewportChange = true;
		}
		else if( pUsingRT && ( pUsingRT->width() != pRT->width() || pUsingRT->height() != pRT->height() ) )
		{
			pRT->m_bViewportChange = true;
		}
		else
		{
			pRT->m_bViewportChange = false;
		}

		m_currentRenderTarget = _id;

		// 切换RenderTarget，dirty 状态机纹理槽信息
		Renderer::instance()->dirtyTexSlot();

		return getRenderTargetByID(_id)->beginRender(_clearColor, _backgroundColor, _clearDepth, _depthValue, _clearStencil, stencilValue);
	}

	// 当屏幕大小改变时调用 
	void RenderTargetManager::onScreensizeChanged( ui32 _width, ui32 _height )
	{
		// 遍历所有(阴影图除外)
		for (RenderTargetMap::iterator bit = m_renderTargets.begin(); bit != m_renderTargets.end(); ++bit)
		{
			RenderTarget* pRenderTarget = bit->second;
			if (pRenderTarget)
			{
				switch (pRenderTarget->id())
				{
				case RTI_DefaultBackBuffer: pRenderTarget->onResize(_width, _height); break;
				}
			}
		}
	}

	// 创建渲染目标
	RenderTarget* RenderTargetManager::createRenderTarget(ui32 _id, ui32 _width, ui32 _height, PixelFormat _pixelFormat, RenderTarget::Options option)
	{
		RenderTargetMap::iterator iter = m_renderTargets.find(_id);
		if (iter != m_renderTargets.end())
		{
			EchoLogError("Rendertarget [%d] is already created.", _id);
			return iter->second;
		}

		if (_pixelFormat == PF_RGBA16_FLOAT && !Renderer::instance()->getDeviceFeatures().supportHFColorBf1())
		{
			_pixelFormat = PF_RGBA8_UNORM;
			EchoLogInfo("Device could not support PF_RGBA16_FLOAT(GL_EXT_color_buffer_half_float) use PF_RGBA8_UNORM!(Rendertarget[%d])",_id);
		}

		RenderTarget* pNewRenderTarget = Renderer::instance()->createRenderTarget(_id, _width, _height, _pixelFormat, option);

		if (!pNewRenderTarget)
		{
			EchoLogError("Allocate RenderTarget Failed !");
			return NULL;
		}

		if (option.depthTarget)
		{
			pNewRenderTarget->reusageDepthTarget(option.depthTarget);
		}

		if (!pNewRenderTarget->create())
		{
			EchoLogError("RenderTarget::create Failed !");
			EchoSafeDelete(pNewRenderTarget, RenderTarget);
			return NULL;
		}

		m_renderTargets.insert(RenderTargetMap::value_type(_id, pNewRenderTarget));

		if (Renderer::instance()->isEnableFrameProfile())
		{
			ui32 size = pNewRenderTarget->getMemorySize();
			Renderer::instance()->getFrameState().incrRendertargetSize(size);
		}

		return pNewRenderTarget;
	}

	// 根据ID获取渲染目标
	RenderTarget* RenderTargetManager::getRenderTargetByID(ui32 _id)
	{
		RenderTargetMap::iterator fit = m_renderTargets.find(_id);
		if (fit == m_renderTargets.end())
		{
			EchoLogError("Could not found RenderTarget[%d]", _id);

			return NULL;
		}

		return fit->second;
	}

	bool RenderTargetManager::endRenderTarget(ui32 _id)
	{
		EchoAssert(m_currentRenderTarget == _id);

		return getRenderTargetByID(_id)->endRender();
	}
	
	bool RenderTargetManager::invalidate(ui32 _id, bool invalidateColor, bool invalidateDepth, bool invalidateStencil)
	{
		//EchoAssert( m_inUsingRenderTarget = RTI_End );

		//return getRenderTargetByID(_id)->invalidateFrameBuffer(invalidateColor, invalidateDepth, invalidateStencil);
		return true;
	}
}
