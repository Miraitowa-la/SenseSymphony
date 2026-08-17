export class FrameBuffer {
	constructor() {
		this.rest = ''
	}

	push(chunk) {
		this.rest += String(chunk || '')
		const lines = this.rest.split(/\r?\n/)
		this.rest = lines.pop()
		return lines.map(parseFrame).filter(Boolean)
	}
}

function parseFrame(line) {
	const fields = line.trim().split(',')
	if (!line.trim() || (fields[0] !== 'M1' && fields[0] !== 'M2')) return null
	if (fields[0] === 'M2') {
		if (fields[1] === 'SONGS' && fields.length === 4) return { type: 'mode2_songs', count: number(fields[2]), selectedId: number(fields[3]) }
		if (fields[1] === 'SONG' && fields.length === 4) return { type: 'mode2_song', id: number(fields[2]), name: fields[3] }
		if (fields[1] === 'LIST' && fields.length === 3) return { type: 'mode2_list', count: number(fields[2]) }
		if (fields[1] === 'ITEM' && fields.length === 12) return { type: 'mode2_item', slot: number(fields[2]), sequence: number(fields[3]), songId: number(fields[4]), score: number(fields[5]), maxCombo: number(fields[6]), accuracy: number(fields[7]), perfect: number(fields[8]), great: number(fields[9]), good: number(fields[10]), miss: number(fields[11]) }
		if (fields[1] === 'OK' && fields[2] === 'UPLOAD_BEGIN' && fields.length === 3) return { type: 'mode2_upload_begin_ok' }
		if (fields[1] === 'OK' && fields[2] === 'NOTE' && fields.length === 4) return { type: 'mode2_upload_note_ok', index: number(fields[3]) }
		if (fields[1] === 'OK' && fields[2] === 'UPLOAD' && fields.length === 4) return { type: 'mode2_upload_ok', songId: number(fields[3]) }
		if (fields[1] === 'OK' && fields[2] === 'SONG_DELETE' && fields.length === 4) return { type: 'mode2_song_delete_ok', songId: number(fields[3]) }
		if (fields[1] === 'OK' && fields[2] === 'CANCEL' && fields.length === 3) return { type: 'mode2_upload_cancel_ok' }
		if (fields[1] === 'OK' && fields[2] === 'SELECT' && fields.length === 4) return { type: 'mode2_select_ok', id: number(fields[3]) }
		if (fields[1] === 'OK' && fields[2] === 'DELETE' && fields.length === 4) return { type: 'mode2_delete_ok', slot: number(fields[3]) }
		if (fields[1] === 'ERR' && fields.length === 3) return { type: 'mode2_error', reason: fields[2] }
		if (fields[1] === 'END' && fields.length === 2) return { type: 'mode2_end' }
		return null
	}
	if (fields[1] === 'LIST' && fields.length === 3) return { type: 'list', count: number(fields[2]) }
	if (fields[1] === 'ITEM' && fields.length === 5) return { type: 'item', slot: number(fields[2]), sequence: number(fields[3]), eventCount: number(fields[4]) }
	if (fields[1] === 'REC' && fields.length === 5) return { type: 'record', slot: number(fields[2]), sequence: number(fields[3]), eventCount: number(fields[4]) }
	if (fields[1] === 'EV' && fields.length === 7) return { type: 'event', timeMs: number(fields[2]), band: number(fields[3]), note: number(fields[4]), x: number(fields[5]), y: number(fields[6]) }
	if (fields[1] === 'OK' && fields[2] === 'DELETE' && fields.length === 4) return { type: 'delete_ok', slot: number(fields[3]) }
	if (fields[1] === 'ERR' && fields.length === 3) return { type: 'error', reason: fields[2] }
	if (fields[1] === 'END' && fields.length === 2) return { type: 'end' }
	return null
}

function number(value) {
	const parsed = Number(value)
	return Number.isFinite(parsed) ? parsed : 0
}
