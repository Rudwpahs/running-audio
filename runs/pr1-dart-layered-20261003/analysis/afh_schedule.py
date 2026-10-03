M=(1<<64)-1
def mix64(x):
    x=(x+0x9E3779B97F4A7C15)&M
    x=((x^(x>>30))*0xBF58476D1CE4E5B9)&M
    x=((x^(x>>27))*0x94D049BB133111EB)&M
    return x^(x>>31)
SEED=0x5052314441525431; MAPV=1; BITS=(1<<40)-1
def seedFor(ep):
    s=mix64(SEED); s^=mix64((MAPV<<32)&M); s^=mix64(BITS); s^=mix64(ep); return mix64(s)
def perm(ep):
    ch=list(range(40)); st=seedFor(ep)
    for i in range(39,0,-1):
        st=(st+0x9E3779B97F4A7C15)&M; z=st
        z=((z^(z>>30))*0xBF58476D1CE4E5B9)&M; z=((z^(z>>27))*0x94D049BB133111EB)&M; z^=z>>31
        j=(z>>32)%(i+1); ch[i],ch[j]=ch[j],ch[i]
    return ch
_c={}
def chan(seq):
    ep,pos=divmod(seq,40)
    if ep not in _c:
        a=perm(ep)
        if ep>0:
            p=perm(ep-1)
            if a[0]==p[39]: a[0],a[1]=a[1],a[0]
        _c[ep]=a
    return _c[ep][pos]
if __name__=='__main__':
    print(','.join(str(chan(i)) for i in range(48)))
