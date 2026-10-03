"""Subject mask for Shaderixel's Select > Subject.

    python segment.py input.png output_mask.png [model]

Uses rembg (models downloaded on first use into ~/.u2net). Exit codes:
0 ok, 2 bad arguments, 3 rembg not installed, 4 failure.
Install: python -m pip install rembg onnxruntime   (onnxruntime-gpu for CUDA)
"""
import sys


def main():
    if len(sys.argv) < 3:
        print('usage: segment.py input.png output_mask.png [model]')
        return 2
    try:
        from PIL import Image
        from rembg import new_session, remove
    except ImportError as e:
        print('missing module:', e)
        return 3
    model = sys.argv[3] if len(sys.argv) > 3 else 'isnet-general-use'
    try:
        session = new_session(model)
        img = Image.open(sys.argv[1]).convert('RGB')
        mask = remove(img, session=session, only_mask=True, post_process_mask=True)
        mask.convert('L').save(sys.argv[2])
    except Exception as e:  # model download, inference...
        print('segmentation failed:', e)
        return 4
    return 0


if __name__ == '__main__':
    sys.exit(main())
