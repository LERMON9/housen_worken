#pragma once

#include <wrl.h>

namespace DX
{
	// Вспомогательный класс для синхронизации анимации и симуляции.
	class StepTimer
	{
	public:
		StepTimer() : 
			m_elapsedTicks(0),
			m_totalTicks(0),
			m_leftOverTicks(0),
			m_frameCount(0),
			m_framesPerSecond(0),
			m_framesThisSecond(0),
			m_qpcSecondCounter(0),
			m_isFixedTimeStep(false),
			m_targetElapsedTicks(TicksPerSecond / 60)
		{
			if (!QueryPerformanceFrequency(&m_qpcFrequency))
			{
				throw ref new Platform::FailureException();
			}

			if (!QueryPerformanceCounter(&m_qpcLastTime))
			{
				throw ref new Platform::FailureException();
			}

			// При инициализации задайте максимальную разницу равной 1/10 секунды.
			m_qpcMaxDelta = m_qpcFrequency.QuadPart / 10;
		}

		// Получение времени, прошедшего с момента предыдущего вызова функции Update.
		uint64 GetElapsedTicks() const						{ return m_elapsedTicks; }
		double GetElapsedSeconds() const					{ return TicksToSeconds(m_elapsedTicks); }

		// Получение полного времени с момента запуска программы.
		uint64 GetTotalTicks() const						{ return m_totalTicks; }
		double GetTotalSeconds() const						{ return TicksToSeconds(m_totalTicks); }

		// Получение общего числа обновлений с момента запуска программы.
		uint32 GetFrameCount() const						{ return m_frameCount; }

		// Получение текущей частоты кадров.
		uint32 GetFramesPerSecond() const					{ return m_framesPerSecond; }

		// Установка используемого режима временного шага: фиксированный или переменный.
		void SetFixedTimeStep(bool isFixedTimestep)			{ m_isFixedTimeStep = isFixedTimestep; }

		// Установка частоты вызовов Update в режиме фиксированного временного шага.
		void SetTargetElapsedTicks(uint64 targetElapsed)	{ m_targetElapsedTicks = targetElapsed; }
		void SetTargetElapsedSeconds(double targetElapsed)	{ m_targetElapsedTicks = SecondsToTicks(targetElapsed); }

		// Целый формат представляет время с использованием 10 000 000 тактов в секунду.
		static const uint64 TicksPerSecond = 10000000;

		static double TicksToSeconds(uint64 ticks)			{ return static_cast<double>(ticks) / TicksPerSecond; }
		static uint64 SecondsToTicks(double seconds)		{ return static_cast<uint64>(seconds * TicksPerSecond); }

		// После преднамеренного разрыва времени (например, при блокирующей операции ввода-вывода)
		// вызовите эту функцию, чтобы логика с фиксированным временным шагом не пыталась произвести набор компенсирующих 
		// вызовов Update.

		void ResetElapsedTime()
		{
			if (!QueryPerformanceCounter(&m_qpcLastTime))
			{
				throw ref new Platform::FailureException();
			}

			m_leftOverTicks = 0;
			m_framesPerSecond = 0;
			m_framesThisSecond = 0;
			m_qpcSecondCounter = 0;
		}

		// Состояние таймера можно обновить, вызывая указанную функцию Update требуемое число раз.
		template<typename TUpdate>
		void Tick(const TUpdate& update)
		{
			// Запрос текущего времени.
			LARGE_INTEGER currentTime;

			if (!QueryPerformanceCounter(&currentTime))
			{
				throw ref new Platform::FailureException();
			}

			uint64 timeDelta = currentTime.QuadPart - m_qpcLastTime.QuadPart;

			m_qpcLastTime = currentTime;
			m_qpcSecondCounter += timeDelta;

			// Выполните приведение слишком больших значений разницы времени (например, после приостановки в отладчике).
			if (timeDelta > m_qpcMaxDelta)
			{
				timeDelta = m_qpcMaxDelta;
			}

			// Преобразуйте единицы QPC в канонический формат отсчетов. Переполнение невозможно из-за предыдущего приведения.
			timeDelta *= TicksPerSecond;
			timeDelta /= m_qpcFrequency.QuadPart;

			uint32 lastFrameCount = m_frameCount;

			if (m_isFixedTimeStep)
			{
				// Логика обновления с фиксированным временным шагом

				// Если выполняемое приложение приблизилось к целевому прошедшему времени (в пределах 1/4 мс), просто выполните приведение
				// часов для точного соответствия целевому значению. Это исключает накопление небольших и несущественных ошибок
				// с течением времени. Без такого приведения игра, которая запросила фиксированное обновление с частотой 60 кадров/с,
				// работающая с включенной вертикальной синхронизацией на экране 59.94 NTSC, постепенно
				// накапливает достаточно небольших ошибок для пропуска кадра. Лучше просто округлять 
				// небольшие отклонения до нуля, обеспечивая плавность работы.

				if (abs(static_cast<int64>(timeDelta - m_targetElapsedTicks)) < TicksPerSecond / 4000)
				{
					timeDelta = m_targetElapsedTicks;
				}

				m_leftOverTicks += timeDelta;

				while (m_leftOverTicks >= m_targetElapsedTicks)
				{
					m_elapsedTicks = m_targetElapsedTicks;
					m_totalTicks += m_targetElapsedTicks;
					m_leftOverTicks -= m_targetElapsedTicks;
					m_frameCount++;

					update();
				}
			}
			else
			{
				// Логика обновления с переменным временным шагом.
				m_elapsedTicks = timeDelta;
				m_totalTicks += timeDelta;
				m_leftOverTicks = 0;
				m_frameCount++;

				update();
			}

			// Отслеживайте текущую частоту кадров.
			if (m_frameCount != lastFrameCount)
			{
				m_framesThisSecond++;
			}

			if (m_qpcSecondCounter >= static_cast<uint64>(m_qpcFrequency.QuadPart))
			{
				m_framesPerSecond = m_framesThisSecond;
				m_framesThisSecond = 0;
				m_qpcSecondCounter %= m_qpcFrequency.QuadPart;
			}
		}

	private:
		// В исходных данных о времени используются единицы QPC.
		LARGE_INTEGER m_qpcFrequency;
		LARGE_INTEGER m_qpcLastTime;
		uint64 m_qpcMaxDelta;

		// В производных данных о времени используется канонический формат отсчетов.
		uint64 m_elapsedTicks;
		uint64 m_totalTicks;
		uint64 m_leftOverTicks;

		// Члены для отслеживания текущей частоты кадров.
		uint32 m_frameCount;
		uint32 m_framesPerSecond;
		uint32 m_framesThisSecond;
		uint64 m_qpcSecondCounter;

		// Члены для настройки режима фиксированного временного шага.
		bool m_isFixedTimeStep;
		uint64 m_targetElapsedTicks;
	};
}
