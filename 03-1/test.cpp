CREATE FUNCTION dbo.IsLineReversedRobust (
    @original geometry,
    @clipped geometry,
    @tolerance float = 0.1  -- tolerance 기본값 증가
)
RETURNS int
AS
BEGIN
    DECLARE @orig_points TABLE (
        idx int,
        point geometry
    );
    
    DECLARE @clip_points TABLE (
        idx int,
        point geometry
    );
    
    -- 원본과 잘린 라인의 점들 추출
    DECLARE @i int = 1;
    WHILE @i <= @original.STNumPoints()
    BEGIN
        INSERT INTO @orig_points (idx, point)
        VALUES (@i, @original.STPointN(@i));
        SET @i = @i + 1;
    END;
    
    SET @i = 1;
    WHILE @i <= @clipped.STNumPoints()
    BEGIN
        INSERT INTO @clip_points (idx, point)
        VALUES (@i, @clipped.STPointN(@i));
        SET @i = @i + 1;
    END;

    -- 점 매칭 시도
    DECLARE @matches TABLE (
        orig_idx int,
        clip_idx int,
        distance float
    );
    
    -- 모든 가능한 매칭 쌍 찾기
    INSERT INTO @matches
    SELECT 
        op.idx,
        cp.idx,
        op.point.STDistance(cp.point) as distance
    FROM @orig_points op
    CROSS APPLY (
        SELECT idx, point
        FROM @clip_points
        WHERE point.STDistance(op.point) <= @tolerance
    ) cp;

    -- 매칭이 없는 경우 대체 로직 사용
    IF NOT EXISTS (SELECT 1 FROM @matches)
    BEGIN
        -- 시작점과 끝점의 벡터로 방향 판단
        DECLARE @orig_vector_x float = @original.STEndPoint().STX - @original.STStartPoint().STX;
        DECLARE @orig_vector_y float = @original.STEndPoint().STY - @original.STStartPoint().STY;
        DECLARE @clip_vector_x float = @clipped.STEndPoint().STX - @clipped.STStartPoint().STX;
        DECLARE @clip_vector_y float = @clipped.STEndPoint().STY - @clipped.STStartPoint().STY;
        
        DECLARE @dot_product float = @orig_vector_x * @clip_vector_x + @orig_vector_y * @clip_vector_y;
        
        RETURN CASE WHEN @dot_product >= 0 THEN 1 ELSE -1 END;
    END;

    -- 첫 번째와 마지막 매칭 찾기
    DECLARE @first_match TABLE (
        orig_idx int,
        clip_idx int
    );
    
    INSERT INTO @first_match
    SELECT TOP 1 orig_idx, clip_idx
    FROM @matches
    ORDER BY distance, orig_idx;

    DECLARE @last_match TABLE (
        orig_idx int,
        clip_idx int
    );
    
    INSERT INTO @last_match
    SELECT TOP 1 orig_idx, clip_idx
    FROM @matches
    WHERE NOT EXISTS (
        SELECT 1 FROM @first_match 
        WHERE orig_idx = @matches.orig_idx
    )
    ORDER BY distance, orig_idx DESC;

    -- 방향성 판단
    DECLARE @first_orig_idx int, @first_clip_idx int;
    DECLARE @last_orig_idx int, @last_clip_idx int;
    
    SELECT @first_orig_idx = orig_idx, @first_clip_idx = clip_idx
    FROM @first_match;
    
    SELECT @last_orig_idx = orig_idx, @last_clip_idx = clip_idx
    FROM @last_match;
    
    RETURN 
        CASE 
            WHEN SIGN(@last_orig_idx - @first_orig_idx) = 
                 SIGN(@last_clip_idx - @first_clip_idx) THEN 1
            ELSE -1
        END;
END;

-- 사용 예시:
/*
-- 남쪽으로 내려가는 거의 일직선 테스트
DECLARE @southward geometry = geometry::STLineFromText('LINESTRING(0 10, 0.001 8, 0.002 6, 0.001 4, 0 2)', 0);
DECLARE @clipped geometry = geometry::STLineFromText('LINESTRING(0.001 8, 0.002 6, 0.001 4)', 0);
SELECT dbo.IsLineReversedRobust(@southward, @clipped);
*/