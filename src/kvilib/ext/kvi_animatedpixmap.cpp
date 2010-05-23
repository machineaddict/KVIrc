//=============================================================================
//
//   File : kvi_animatedpixmap.cpp
//   Creation date : Wed Jul 30 2008 01:45:21 CEST by Alexey Uzhva
//
//   This file is part of the KVirc irc client distribution
//   Copyright (C) 2008 Alexey Uzhva (wizard at opendoor dot ru)
//
//   This program is FREE software. You can redistribute it and/or
//   modify it under the terms of the GNU General Public License
//   as published by the Free Software Foundation; either version 2
//   of the License, or (at your opinion) any later version.
//
//   This program is distributed in the HOPE that it will be USEFUL,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//   See the GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with this program. If not, write to the Free Software Foundation,
//   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
//=============================================================================

#include "kvi_animatedpixmap.h"
#include "kvi_settings.h"
#include <QImageReader>
#include <QTime>
#include <QHash>
#include <QMutexLocker>

KviAnimatedPixmap::KviAnimatedPixmap(QString fileName)
	:m_szFileName(fileName)
{
	m_bStarted            = 0;
	m_uCurrentFrameNumber = 0;

	m_lFrames=KviAnimatedPixmapCache::load(fileName);
	start();
}

KviAnimatedPixmap::KviAnimatedPixmap(KviAnimatedPixmap* source)
	:m_szFileName(source->m_szFileName),
	m_lFrames(source->m_lFrames),
	m_uCurrentFrameNumber(source->m_uCurrentFrameNumber),
	m_bStarted(source->m_bStarted) //keep started state
{
	m_lFrames->refs++;

	//restore started state
	if(isStarted() && (framesCount()>1))
	{
		KviAnimatedPixmapCache::scheduleFrameChange(m_lFrames->at(m_uCurrentFrameNumber).delay,this);
	}
}

KviAnimatedPixmap::~KviAnimatedPixmap()
{
	KviAnimatedPixmapCache::notifyDelete(this);
	KviAnimatedPixmapCache::free(m_lFrames);
}

void KviAnimatedPixmap::start()
{
	if(!isStarted() && (framesCount()>1))
	{
		KviAnimatedPixmapCache::scheduleFrameChange(m_lFrames->at(m_uCurrentFrameNumber).delay,this);
		m_bStarted = true;
	}
}

void KviAnimatedPixmap::stop()
{
	if(isStarted())
	{
		m_bStarted = false;
	}
}
#include <stdio.h>
void KviAnimatedPixmap::nextFrame(bool bScheduleNext)
{
	m_uCurrentFrameNumber++;
	//Ensure, that we are not out of bounds
	m_uCurrentFrameNumber %= m_lFrames->count();

	//run timer again
	if(m_bStarted && bScheduleNext)
	{
		emit(frameChanged());
		KviAnimatedPixmapCache::scheduleFrameChange(m_lFrames->at(m_uCurrentFrameNumber).delay,this);
	}
}

void KviAnimatedPixmap::resize(QSize newSize,Qt::AspectRatioMode ratioMode)
{
	QSize curSize(size());
	curSize.scale(newSize,ratioMode);

	m_lFrames = KviAnimatedPixmapCache::resize(m_lFrames,curSize);
}