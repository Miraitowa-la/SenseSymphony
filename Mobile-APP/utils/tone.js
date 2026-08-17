const bands = ['low', 'mid', 'high']
const notes = ['do', 're', 'mi', 'fa', 'sol', 'la', 'si']
let timers = []
let players = []
let finished = null

function sourceFor(event) {
	const band = bands[event.band]
	const note = notes[event.note]
	return band && note ? `/static/audio/mode1/face_${band}_${event.note + 1}_${note}.wav` : ''
}

function play(events, onComplete) {
	stop()
	if (!events.length || typeof uni === 'undefined' || !uni.createInnerAudioContext) return false
	let remaining = events.length
	finished = onComplete
	const start = events[0].timeMs
	events.forEach((event) => {
		timers.push(setTimeout(() => playEvent(event, () => {
			remaining--
			if (!remaining && finished) {
				const complete = finished
				finished = null
				complete()
			}
		}), Math.max(0, event.timeMs - start)))
	})
	return true
}

function playEvent(event, done) {
	const src = sourceFor(event)
	if (!src) return done()
	const player = uni.createInnerAudioContext()
	players.push(player)
	let closed = false
	const close = () => {
		if (closed) return
		closed = true
		players = players.filter((item) => item !== player)
		player.destroy()
		done()
	}
	player.src = src
	player.onEnded(close)
	player.onError(close)
	player.play()
}

function stop() {
	timers.forEach(clearTimeout)
	timers = []
	finished = null
	players.forEach((player) => {
		try { player.stop(); player.destroy() } catch (error) {}
	})
	players = []
}

export { play, stop, sourceFor }
