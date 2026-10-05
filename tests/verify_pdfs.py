"""Optional PDF artifact check: pip install pypdf pdfplumber."""
from pathlib import Path
import sys
from pypdf import PdfReader
import pdfplumber

root=Path(sys.argv[1] if len(sys.argv)>1 else 'output/pdf')
expected={'chessboard-A4.pdf':(297,210),'charuco-custom.pdf':(320.5,240.25),'aprilgrid-A3.pdf':(297,420)}
for name,(width,height) in expected.items():
    path=root/name
    page=PdfReader(path,strict=True).pages[0]
    assert abs(float(page.mediabox.width)*25.4/72-width)<1e-7
    assert abs(float(page.mediabox.height)*25.4/72-height)<1e-7
    assert not page.images, 'Expected vector-only PDF'
    with pdfplumber.open(path) as document:
        rects=document.pages[0].rects
        black=[r for r in rects if r.get('non_stroking_color') in [(0,0,0),(0,),0]]
        assert black
        if name=='chessboard-A4.pdf':
            assert len(black)==32
            for r in black:
                assert abs(r['width']*25.4/72-20)<1e-5
                assert abs(r['height']*25.4/72-20)<1e-5
            assert abs(min(r['x0'] for r in black)*25.4/72-58.5)<1e-5
            assert abs(min(r['top'] for r in black)*25.4/72-35)<1e-5
    print(f'PASS {name}: {width} x {height} mm, vector content')
