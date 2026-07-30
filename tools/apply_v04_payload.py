from pathlib import Path
import base64, io, tarfile, zlib

root=Path(__file__).resolve().parents[1]
parts=sorted((root/'tools').glob('v04-payload-*.part'))
payload=''.join(''.join(p.read_text(encoding='ascii').split()) for p in parts)
data=zlib.decompress(base64.b85decode(payload.encode('ascii')))
with tarfile.open(fileobj=io.BytesIO(data), mode='r:') as archive:
    for member in archive.getmembers():
        target=(root/member.name).resolve()
        if root not in target.parents:
            raise RuntimeError(f'unsafe payload path: {member.name}')
        target.parent.mkdir(parents=True, exist_ok=True)
        source=archive.extractfile(member)
        if source is None:
            raise RuntimeError(f'missing payload member: {member.name}')
        target.write_bytes(source.read())
print(f'Applied EpochRunner v0.4 source payload: {len(archive.getmembers())} files')
